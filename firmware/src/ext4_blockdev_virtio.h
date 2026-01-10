/*
 * ext4_blockdev_virtio.h - lwext4 block device adapter for VirtIO
 */

#ifndef EXT4_BLOCKDEV_VIRTIO_H
#define EXT4_BLOCKDEV_VIRTIO_H

#include <ext4_blockdev.h>

/* Get the VirtIO block device instance for lwext4 */
struct ext4_blockdev *ext4_blockdev_virtio_get(void);

/* Get block device name (for registration) */
const char *ext4_blockdev_virtio_name(void);

#endif /* EXT4_BLOCKDEV_VIRTIO_H */
