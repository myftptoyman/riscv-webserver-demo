/*
 * virtio_net.c - VirtIO FIFO network driver for lwIP
 *
 * Uses the VirtIO FIFO device in Spike for network communication.
 * Frames are encapsulated with a 2-byte length prefix.
 */

#include "virtio_net.h"
#include "platform.h"
#include "console.h"
#include "plic.h"

#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "lwip/pbuf.h"
#include "netif/ethernet.h"
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

/* VirtIO status bits */
#define VIRTIO_STATUS_ACK           0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FEATURES_OK   0x08

/* Virtqueue indices */
#define QUEUE_TX 0
#define QUEUE_RX 1

/* Queue configuration */
#define QUEUE_SIZE 16  /* Must be power of 2 */

/* VirtIO descriptor flags */
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

/* Buffer size (includes 2-byte length prefix) */
#define BUF_SIZE 2048

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

/* TX and RX buffers */
static uint8_t tx_buffers[QUEUE_SIZE][BUF_SIZE] __attribute__((aligned(4096)));
static uint8_t rx_buffers[QUEUE_SIZE][BUF_SIZE] __attribute__((aligned(4096)));

/* Virtqueues */
static struct virtqueue tx_queue __attribute__((aligned(4096)));
static struct virtqueue rx_queue __attribute__((aligned(4096)));

/* Network interface */
static struct netif virtio_netif;

/* MAC address: locally administered, unicast */
static const uint8_t mac_addr[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

/* MMIO access macros */
#define VIRTIO_READ32(off)     MMIO_READ32(VIRTIO_FIFO_BASE + (off))
#define VIRTIO_WRITE32(off, v) MMIO_WRITE32(VIRTIO_FIFO_BASE + (off), (v))

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

/* Set up RX buffers for receiving */
static void setup_rx_buffers(void) {
    for (int i = 0; i < QUEUE_SIZE / 2; i++) {
        uint16_t desc_idx = rx_queue.free_head;
        rx_queue.free_head = rx_queue.descs[desc_idx].next;
        rx_queue.num_free--;

        rx_queue.descs[desc_idx].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_queue.descs[desc_idx].len = BUF_SIZE;
        rx_queue.descs[desc_idx].flags = VRING_DESC_F_WRITE;
        rx_queue.descs[desc_idx].next = 0;

        rx_queue.avail.ring[rx_queue.avail.idx % QUEUE_SIZE] = desc_idx;
        mb();
        rx_queue.avail.idx++;
    }

    /* Notify device */
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_NOTIFY, QUEUE_RX);
}

/* Initialize VirtIO device */
static int virtio_init(void) {
    uint32_t magic, version, device_id;

    /* Check magic number */
    magic = VIRTIO_READ32(VIRTIO_MMIO_MAGIC);
    if (magic != 0x74726976) {
        console_printf("VirtIO: bad magic 0x%x\n", magic);
        return -1;
    }

    /* Check version */
    version = VIRTIO_READ32(VIRTIO_MMIO_VERSION);
    if (version != 2) {
        console_printf("VirtIO: unsupported version %d\n", version);
        return -1;
    }

    /* Check device ID (0x1F for FIFO) */
    device_id = VIRTIO_READ32(VIRTIO_MMIO_DEVICE_ID);
    if (device_id != 0x1F) {
        console_printf("VirtIO: wrong device ID 0x%x\n", device_id);
        return -1;
    }

    console_printf("VirtIO FIFO device found\n");

    /* Reset device */
    VIRTIO_WRITE32(VIRTIO_MMIO_STATUS, 0);

    /* Acknowledge */
    VIRTIO_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK);

    /* Driver */
    VIRTIO_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    /* Accept VERSION_1 feature */
    VIRTIO_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    VIRTIO_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES, 1);  /* VIRTIO_F_VERSION_1 */
    VIRTIO_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_DRIVER_FEATURES, 0);

    /* Features OK */
    VIRTIO_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                                        VIRTIO_STATUS_FEATURES_OK);

    /* Initialize TX queue (queue 0) */
    init_queue(&tx_queue);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_SEL, QUEUE_TX);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(uintptr_t)tx_queue.descs);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(uintptr_t)&tx_queue.avail);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(uintptr_t)&tx_queue.used);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_USED_HIGH, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_READY, 1);

    /* Initialize RX queue (queue 1) */
    init_queue(&rx_queue);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_SEL, QUEUE_RX);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_NUM, QUEUE_SIZE);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(uintptr_t)rx_queue.descs);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(uintptr_t)&rx_queue.avail);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(uintptr_t)&rx_queue.used);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_USED_HIGH, 0);
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_READY, 1);

    /* Set up RX buffers */
    setup_rx_buffers();

    /* Driver OK */
    VIRTIO_WRITE32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                                        VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    console_printf("VirtIO FIFO initialized\n");
    return 0;
}

/* Send Ethernet frame via TX queue */
static err_t virtio_net_output(struct netif *netif, struct pbuf *p) {
    (void)netif;

    if (p->tot_len > BUF_SIZE - 2) {
        console_printf("TX: frame too large (%d bytes)\n", p->tot_len);
        return ERR_MEM;
    }

    if (tx_queue.num_free == 0) {
        console_printf("TX: no free descriptors\n");
        return ERR_MEM;
    }

    /* Get a free descriptor */
    uint16_t desc_idx = tx_queue.free_head;
    tx_queue.free_head = tx_queue.descs[desc_idx].next;
    tx_queue.num_free--;

    /* Use corresponding buffer */
    uint8_t *buf = tx_buffers[desc_idx];

    /* Frame format: 2-byte length prefix + Ethernet frame */
    buf[0] = (p->tot_len >> 8) & 0xFF;
    buf[1] = p->tot_len & 0xFF;

    /* Copy pbuf chain to buffer */
    uint16_t offset = 2;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
        memcpy(buf + offset, q->payload, q->len);
        offset += q->len;
    }

    /* Set up descriptor */
    tx_queue.descs[desc_idx].addr = (uint64_t)(uintptr_t)buf;
    tx_queue.descs[desc_idx].len = offset;
    tx_queue.descs[desc_idx].flags = 0;

    /* Add to available ring */
    tx_queue.avail.ring[tx_queue.avail.idx % QUEUE_SIZE] = desc_idx;
    mb();
    tx_queue.avail.idx++;

    /* Notify device */
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_NOTIFY, QUEUE_TX);

    return ERR_OK;
}

/* Process received frames */
static void virtio_net_input(struct netif *netif) {
    while (rx_queue.last_used_idx != rx_queue.used.idx) {
        uint16_t used_idx = rx_queue.last_used_idx % QUEUE_SIZE;
        uint16_t desc_idx = rx_queue.used.ring[used_idx].id;
        uint32_t len = rx_queue.used.ring[used_idx].len;

        uint8_t *buf = rx_buffers[desc_idx % (QUEUE_SIZE / 2)];

        if (len >= 2) {
            /* Extract frame length from prefix */
            uint16_t frame_len = (buf[0] << 8) | buf[1];

            if (frame_len > 0 && frame_len <= len - 2) {
                /* Allocate pbuf */
                struct pbuf *p = pbuf_alloc(PBUF_RAW, frame_len, PBUF_POOL);
                if (p != NULL) {
                    /* Copy frame data (skip 2-byte length prefix) */
                    pbuf_take(p, buf + 2, frame_len);

                    /* Pass to lwIP */
                    if (netif->input(p, netif) != ERR_OK) {
                        pbuf_free(p);
                    }
                }
            }
        }

        /* Return buffer to available ring */
        rx_queue.descs[desc_idx].addr = (uint64_t)(uintptr_t)buf;
        rx_queue.descs[desc_idx].len = BUF_SIZE;
        rx_queue.descs[desc_idx].flags = VRING_DESC_F_WRITE;

        rx_queue.avail.ring[rx_queue.avail.idx % QUEUE_SIZE] = desc_idx;
        mb();
        rx_queue.avail.idx++;

        rx_queue.last_used_idx++;
    }

    /* Notify device that we've returned buffers */
    VIRTIO_WRITE32(VIRTIO_MMIO_QUEUE_NOTIFY, QUEUE_RX);
}

/* Process completed TX buffers */
static void virtio_net_tx_complete(void) {
    while (tx_queue.last_used_idx != tx_queue.used.idx) {
        uint16_t used_idx = tx_queue.last_used_idx % QUEUE_SIZE;
        uint16_t desc_idx = tx_queue.used.ring[used_idx].id;

        /* Return descriptor to free list */
        tx_queue.descs[desc_idx].next = tx_queue.free_head;
        tx_queue.free_head = desc_idx;
        tx_queue.num_free++;

        tx_queue.last_used_idx++;
    }
}

/* Handle VirtIO interrupt */
void virtio_net_irq_handler(void) {
    uint32_t status = VIRTIO_READ32(VIRTIO_MMIO_INTERRUPT_STATUS);
    VIRTIO_WRITE32(VIRTIO_MMIO_INTERRUPT_ACK, status);

    /* Process TX completions */
    virtio_net_tx_complete();

    /* Process RX */
    virtio_net_input(&virtio_netif);
}

/* lwIP netif initialization callback */
static err_t virtio_netif_init(struct netif *netif) {
    netif->name[0] = 'e';
    netif->name[1] = 't';
    netif->mtu = 1500;
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, mac_addr, 6);
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->output = etharp_output;
    netif->linkoutput = virtio_net_output;

    return ERR_OK;
}

/* Initialize network interface */
struct netif* virtio_net_init(void) {
    ip4_addr_t ipaddr, netmask, gateway;

    /* Initialize VirtIO device */
    if (virtio_init() != 0) {
        return NULL;
    }

    /* Enable PLIC interrupt for VirtIO */
    plic_enable(VIRTIO_FIFO_INT_ID);

    /* Static IP configuration matching SLIRP defaults */
    IP4_ADDR(&ipaddr, 10, 0, 2, 15);     /* Guest IP */
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 10, 0, 2, 2);     /* SLIRP gateway */

    /* Add network interface */
    if (netif_add(&virtio_netif, &ipaddr, &netmask, &gateway,
                  NULL, virtio_netif_init, ethernet_input) == NULL) {
        console_printf("Failed to add netif\n");
        return NULL;
    }

    netif_set_default(&virtio_netif);
    netif_set_up(&virtio_netif);

    console_printf("Network interface up: %d.%d.%d.%d\n",
                   ip4_addr1(&ipaddr), ip4_addr2(&ipaddr),
                   ip4_addr3(&ipaddr), ip4_addr4(&ipaddr));

    return &virtio_netif;
}

/* Poll for network activity (call from main loop) */
void virtio_net_poll(void) {
    /* Check for interrupts and process */
    uint32_t status = VIRTIO_READ32(VIRTIO_MMIO_INTERRUPT_STATUS);
    if (status) {
        VIRTIO_WRITE32(VIRTIO_MMIO_INTERRUPT_ACK, status);
        virtio_net_tx_complete();
        virtio_net_input(&virtio_netif);
    }
}
