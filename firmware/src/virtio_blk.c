/*
 * virtio_blk.c - VirtIO block device driver
 *
 * Provides sector-level read/write access to a virtual disk.
 * Uses VirtIO MMIO transport with single request queue.
 */

#include "virtio_blk.h"
#include "platform.h"
#include "console.h"
#include <string.h>

/* VirtIO MMIO register offsets */
#define VIRTIO_MMIO_MAGIC           0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4
#define VIRTIO_MMIO_CONFIG          0x100

/* VirtIO status bits */
#define VIRTIO_STATUS_ACK           0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FEATURES_OK   0x08

/* VirtIO device IDs */
#define VIRTIO_ID_BLOCK             0x02

/* VirtIO block request types */
#define VIRTIO_BLK_T_IN             0   /* Read */
#define VIRTIO_BLK_T_OUT            1   /* Write */
#define VIRTIO_BLK_T_FLUSH          4   /* Flush */

/* VirtIO block status values */
#define VIRTIO_BLK_S_OK             0
#define VIRTIO_BLK_S_IOERR          1
#define VIRTIO_BLK_S_UNSUPP         2

/* VirtIO descriptor flags */
#define VRING_DESC_F_NEXT           1
#define VRING_DESC_F_WRITE          2

/* Queue configuration */
#define QUEUE_SIZE 16  /* Must be power of 2 */
#define QUEUE_REQUEST 0

/* Maximum sectors per request */
#define MAX_SECTORS_PER_REQ 128

/* VirtIO block request header */
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

/* VirtIO descriptor */
struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

/* Available ring */
struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
} __attribute__((packed));

/* Used ring element */
struct vring_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

/* Used ring */
struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[QUEUE_SIZE];
} __attribute__((packed));

/* Complete virtqueue structure */
struct virtqueue {
    struct vring_desc descs[QUEUE_SIZE] __attribute__((aligned(16)));
    struct vring_avail avail __attribute__((aligned(2)));
    uint8_t _pad[4096 - sizeof(struct vring_desc) * QUEUE_SIZE - sizeof(struct vring_avail)];
    struct vring_used used __attribute__((aligned(4096)));

    uint16_t last_used_idx;
    uint16_t num_free;
    uint16_t free_head;
};

/* Request queue */
static struct virtqueue req_queue __attribute__((aligned(4096)));

/* Request header buffer */
static struct virtio_blk_req req_header __attribute__((aligned(16)));

/* Status byte buffer */
static uint8_t req_status __attribute__((aligned(16)));

/* Data buffer for I/O operations */
static uint8_t data_buffer[MAX_SECTORS_PER_REQ * VIRTIO_BLK_SECTOR_SIZE] __attribute__((aligned(4096)));

/* Device state */
static int blk_initialized = 0;
static uint64_t blk_capacity = 0;
static uint32_t blk_sector_size = VIRTIO_BLK_SECTOR_SIZE;

/* MMIO access macros */
#define BLK_READ32(off)     MMIO_READ32(VIRTIO_BLOCK_BASE + (off))
#define BLK_WRITE32(off, v) MMIO_WRITE32(VIRTIO_BLOCK_BASE + (off), (v))

/* Memory barrier */
#define mb() __asm__ volatile("fence rw, rw" ::: "memory")

/* Initialize a virtqueue */
static void init_queue(struct virtqueue *q) {
    memset(q, 0, sizeof(*q));

    /* Initialize free list */
    for (int i = 0; i < QUEUE_SIZE - 1; i++) {
        q->descs[i].next = i + 1;
    }
    q->descs[QUEUE_SIZE - 1].next = 0xFFFF;

    q->last_used_idx = 0;
    q->num_free = QUEUE_SIZE;
    q->free_head = 0;
}

/* Wait for request completion */
static int wait_for_completion(void) {
    /* Poll for completion */
    while (req_queue.last_used_idx == req_queue.used.idx) {
        /* Spin - in a real system you'd want a timeout */
        mb();
    }

    /* Acknowledge interrupt */
    uint32_t status = BLK_READ32(VIRTIO_MMIO_INTERRUPT_STATUS);
    if (status) {
        BLK_WRITE32(VIRTIO_MMIO_INTERRUPT_ACK, status);
    }

    req_queue.last_used_idx++;
    return 0;
}

/* Submit a block I/O request */
static int submit_request(uint32_t type, uint64_t sector, void *buf, uint32_t len) {
    if (!blk_initialized) {
        return -1;
    }

    /* Set up request header */
    req_header.type = type;
    req_header.reserved = 0;
    req_header.sector = sector;

    /* Get three free descriptors for: header, data, status */
    if (req_queue.num_free < 3) {
        console_printf("virtio-blk: no free descriptors\n");
        return -1;
    }

    uint16_t head = req_queue.free_head;
    uint16_t desc0 = head;
    uint16_t desc1 = req_queue.descs[desc0].next;
    uint16_t desc2 = req_queue.descs[desc1].next;
    req_queue.free_head = req_queue.descs[desc2].next;
    req_queue.num_free -= 3;

    /* Descriptor 0: Request header (device-readable) */
    req_queue.descs[desc0].addr = (uint64_t)(uintptr_t)&req_header;
    req_queue.descs[desc0].len = sizeof(req_header);
    req_queue.descs[desc0].flags = VRING_DESC_F_NEXT;
    req_queue.descs[desc0].next = desc1;

    /* Descriptor 1: Data buffer */
    req_queue.descs[desc1].addr = (uint64_t)(uintptr_t)buf;
    req_queue.descs[desc1].len = len;
    if (type == VIRTIO_BLK_T_IN) {
        /* Read: device writes to buffer */
        req_queue.descs[desc1].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    } else {
        /* Write: device reads from buffer */
        req_queue.descs[desc1].flags = VRING_DESC_F_NEXT;
    }
    req_queue.descs[desc1].next = desc2;

    /* Descriptor 2: Status byte (device-writable) */
    req_status = 0xFF;  /* Invalid status initially */
    req_queue.descs[desc2].addr = (uint64_t)(uintptr_t)&req_status;
    req_queue.descs[desc2].len = 1;
    req_queue.descs[desc2].flags = VRING_DESC_F_WRITE;
    req_queue.descs[desc2].next = 0;

    /* Add to available ring */
    req_queue.avail.ring[req_queue.avail.idx % QUEUE_SIZE] = head;
    mb();
    req_queue.avail.idx++;

    /* Notify device */
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_NOTIFY, QUEUE_REQUEST);

    /* Wait for completion */
    wait_for_completion();

    /* Return descriptors to free list */
    req_queue.descs[desc2].next = req_queue.free_head;
    req_queue.descs[desc1].next = desc2;
    req_queue.descs[desc0].next = desc1;
    req_queue.free_head = desc0;
    req_queue.num_free += 3;

    /* Check status */
    if (req_status != VIRTIO_BLK_S_OK) {
        console_printf("virtio-blk: I/O error, status=%d\n", req_status);
        return -1;
    }

    return 0;
}

int virtio_blk_init(void) {
    uint32_t magic, version, device_id;

    /* Check magic number */
    magic = BLK_READ32(VIRTIO_MMIO_MAGIC);
    if (magic != 0x74726976) {
        /* Device not present */
        return -1;
    }

    /* Check version */
    version = BLK_READ32(VIRTIO_MMIO_VERSION);
    if (version != 2) {
        console_printf("virtio-blk: unsupported version %d\n", version);
        return -1;
    }

    /* Check device ID */
    device_id = BLK_READ32(VIRTIO_MMIO_DEVICE_ID);
    if (device_id != VIRTIO_ID_BLOCK) {
        console_printf("virtio-blk: wrong device ID 0x%x (expected 0x%x)\n",
                       device_id, VIRTIO_ID_BLOCK);
        return -1;
    }

    console_printf("VirtIO block device found\n");

    /* Reset device */
    BLK_WRITE32(VIRTIO_MMIO_STATUS, 0);

    /* Acknowledge */
    BLK_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK);

    /* Driver */
    BLK_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    /* Read configuration: capacity at offset 0 (8 bytes) */
    uint32_t cap_lo = BLK_READ32(VIRTIO_MMIO_CONFIG + 0);
    uint32_t cap_hi = BLK_READ32(VIRTIO_MMIO_CONFIG + 4);
    blk_capacity = ((uint64_t)cap_hi << 32) | cap_lo;

    /* Read block size at offset 20 (4 bytes) */
    uint32_t bsize = BLK_READ32(VIRTIO_MMIO_CONFIG + 20);
    if (bsize > 0) {
        blk_sector_size = bsize;
    }

    console_printf("virtio-blk: capacity=%llu sectors, sector_size=%u\n",
                   (unsigned long long)blk_capacity, blk_sector_size);

    /* Accept VERSION_1 feature */
    BLK_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    BLK_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES, 1);  /* VIRTIO_F_VERSION_1 */
    BLK_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    BLK_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES, 0);

    /* Features OK */
    BLK_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                                    VIRTIO_STATUS_FEATURES_OK);

    /* Initialize request queue */
    init_queue(&req_queue);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_SEL, QUEUE_REQUEST);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(uintptr_t)req_queue.descs);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(uintptr_t)&req_queue.avail);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(uintptr_t)&req_queue.used);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_USED_HIGH, 0);
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_READY, 1);

    /* Driver OK */
    BLK_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                                    VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    blk_initialized = 1;
    console_printf("VirtIO block device initialized\n");
    return 0;
}

int virtio_blk_read(uint64_t sector, void *buf, uint32_t count) {
    if (!blk_initialized || count == 0) {
        return -1;
    }

    if (sector + count > blk_capacity) {
        console_printf("virtio-blk: read past end of disk\n");
        return -1;
    }

    /* Read in chunks if necessary */
    uint8_t *p = (uint8_t *)buf;
    while (count > 0) {
        uint32_t n = (count > MAX_SECTORS_PER_REQ) ? MAX_SECTORS_PER_REQ : count;
        uint32_t len = n * blk_sector_size;

        if (submit_request(VIRTIO_BLK_T_IN, sector, data_buffer, len) != 0) {
            return -1;
        }

        memcpy(p, data_buffer, len);
        p += len;
        sector += n;
        count -= n;
    }

    return 0;
}

int virtio_blk_write(uint64_t sector, const void *buf, uint32_t count) {
    if (!blk_initialized || count == 0) {
        return -1;
    }

    if (sector + count > blk_capacity) {
        console_printf("virtio-blk: write past end of disk\n");
        return -1;
    }

    /* Write in chunks if necessary */
    const uint8_t *p = (const uint8_t *)buf;
    while (count > 0) {
        uint32_t n = (count > MAX_SECTORS_PER_REQ) ? MAX_SECTORS_PER_REQ : count;
        uint32_t len = n * blk_sector_size;

        memcpy(data_buffer, p, len);

        if (submit_request(VIRTIO_BLK_T_OUT, sector, data_buffer, len) != 0) {
            return -1;
        }

        p += len;
        sector += n;
        count -= n;
    }

    return 0;
}

int virtio_blk_flush(void) {
    if (!blk_initialized) {
        return -1;
    }

    /* For flush, we don't need data, just header and status */
    req_header.type = VIRTIO_BLK_T_FLUSH;
    req_header.reserved = 0;
    req_header.sector = 0;

    /* Get two descriptors: header, status */
    if (req_queue.num_free < 2) {
        return -1;
    }

    uint16_t head = req_queue.free_head;
    uint16_t desc0 = head;
    uint16_t desc1 = req_queue.descs[desc0].next;
    req_queue.free_head = req_queue.descs[desc1].next;
    req_queue.num_free -= 2;

    /* Descriptor 0: Request header */
    req_queue.descs[desc0].addr = (uint64_t)(uintptr_t)&req_header;
    req_queue.descs[desc0].len = sizeof(req_header);
    req_queue.descs[desc0].flags = VRING_DESC_F_NEXT;
    req_queue.descs[desc0].next = desc1;

    /* Descriptor 1: Status byte */
    req_status = 0xFF;
    req_queue.descs[desc1].addr = (uint64_t)(uintptr_t)&req_status;
    req_queue.descs[desc1].len = 1;
    req_queue.descs[desc1].flags = VRING_DESC_F_WRITE;
    req_queue.descs[desc1].next = 0;

    /* Submit */
    req_queue.avail.ring[req_queue.avail.idx % QUEUE_SIZE] = head;
    mb();
    req_queue.avail.idx++;
    BLK_WRITE32(VIRTIO_MMIO_QUEUE_NOTIFY, QUEUE_REQUEST);

    /* Wait */
    wait_for_completion();

    /* Return descriptors */
    req_queue.descs[desc1].next = req_queue.free_head;
    req_queue.descs[desc0].next = desc1;
    req_queue.free_head = desc0;
    req_queue.num_free += 2;

    return (req_status == VIRTIO_BLK_S_OK) ? 0 : -1;
}

uint64_t virtio_blk_capacity(void) {
    return blk_capacity;
}

uint32_t virtio_blk_sector_size(void) {
    return blk_sector_size;
}

int virtio_blk_available(void) {
    return blk_initialized;
}
