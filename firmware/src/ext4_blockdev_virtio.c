/*
 * ext4_blockdev_virtio.c - lwext4 block device adapter for VirtIO
 *
 * Bridges the lwext4 block device interface to the VirtIO block driver.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Include our config first, before lwext4's config */
#define CONFIG_USE_DEFAULT_CFG 0

#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>

#include "virtio_blk.h"
#include "console.h"

/* Block size - ext4 typically uses 1024, 2048 or 4096 byte blocks
 * We'll use 512 to match the VirtIO sector size for simplicity */
#define EXT4_BLOCKDEV_BSIZE 512

/* Physical block buffer */
static uint8_t blockdev_ph_bbuf[EXT4_BLOCKDEV_BSIZE];

/* Block device interface callbacks */

static int virtio_blockdev_open(struct ext4_blockdev *bdev)
{
    (void)bdev;

    /* Initialize VirtIO block device if not already done */
    if (!virtio_blk_available()) {
        if (virtio_blk_init() != 0) {
            console_printf("ext4: Failed to initialize VirtIO block device\n");
            return EIO;
        }
    }

    return EOK;
}

static int virtio_blockdev_bread(struct ext4_blockdev *bdev, void *buf,
                                  uint64_t blk_id, uint32_t blk_cnt)
{
    (void)bdev;

    /* Convert block ID to sector number
     * For 512-byte blocks, blk_id == sector number */
    if (virtio_blk_read(blk_id, buf, blk_cnt) != 0) {
        return EIO;
    }

    return EOK;
}

static int virtio_blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
                                   uint64_t blk_id, uint32_t blk_cnt)
{
    (void)bdev;

    if (virtio_blk_write(blk_id, buf, blk_cnt) != 0) {
        return EIO;
    }

    return EOK;
}

static int virtio_blockdev_close(struct ext4_blockdev *bdev)
{
    (void)bdev;

    /* Flush any pending writes */
    virtio_blk_flush();

    return EOK;
}

/* Block device interface structure */
static struct ext4_blockdev_iface virtio_blockdev_iface = {
    .open = virtio_blockdev_open,
    .bread = virtio_blockdev_bread,
    .bwrite = virtio_blockdev_bwrite,
    .close = virtio_blockdev_close,
    .lock = NULL,   /* No locking needed in single-threaded environment */
    .unlock = NULL,
    .ph_bsize = EXT4_BLOCKDEV_BSIZE,
    .ph_bcnt = 0,   /* Will be set during init */
    .ph_bbuf = blockdev_ph_bbuf,
    .ph_refctr = 0,
    .bread_ctr = 0,
    .bwrite_ctr = 0,
    .p_user = NULL,
};

/* Block device instance */
static struct ext4_blockdev virtio_blockdev = {
    .bdif = &virtio_blockdev_iface,
    .part_offset = 0,
    .part_size = 0,  /* Will be set during init */
    .bc = NULL,
    .lg_bsize = 0,
    .lg_bcnt = 0,
    .cache_write_back = 0,
    .fs = NULL,
    .journal = NULL,
};

/* Get the block device instance */
struct ext4_blockdev *ext4_blockdev_virtio_get(void)
{
    /* Initialize block count from VirtIO device capacity */
    if (virtio_blockdev_iface.ph_bcnt == 0) {
        /* First ensure VirtIO block is initialized */
        if (!virtio_blk_available()) {
            if (virtio_blk_init() != 0) {
                console_printf("ext4: VirtIO block device not available\n");
                return NULL;
            }
        }

        uint64_t capacity = virtio_blk_capacity();
        uint32_t sector_size = virtio_blk_sector_size();

        /* Set physical block count */
        virtio_blockdev_iface.ph_bcnt = capacity;
        virtio_blockdev.part_size = capacity * sector_size;

        console_printf("ext4: Block device: %llu blocks, %lu bytes total\n",
                       (unsigned long long)capacity,
                       (unsigned long)virtio_blockdev.part_size);
    }

    return &virtio_blockdev;
}

/* Get block device name (for registration) */
const char *ext4_blockdev_virtio_name(void)
{
    return "virtio0";
}
