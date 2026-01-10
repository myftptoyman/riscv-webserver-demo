/*
 * virtio_blk.h - VirtIO block device driver
 *
 * Provides sector-level read/write access to a virtual disk.
 */

#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#include <stdint.h>
#include <stddef.h>

/* Sector size in bytes */
#define VIRTIO_BLK_SECTOR_SIZE 512

/* Initialize VirtIO block device */
int virtio_blk_init(void);

/* Read sectors from disk
 * sector: starting sector number
 * buf: buffer to read into
 * count: number of sectors to read
 * Returns: 0 on success, negative on error
 */
int virtio_blk_read(uint64_t sector, void *buf, uint32_t count);

/* Write sectors to disk
 * sector: starting sector number
 * buf: buffer containing data to write
 * count: number of sectors to write
 * Returns: 0 on success, negative on error
 */
int virtio_blk_write(uint64_t sector, const void *buf, uint32_t count);

/* Flush disk cache
 * Returns: 0 on success, negative on error
 */
int virtio_blk_flush(void);

/* Get disk capacity in sectors */
uint64_t virtio_blk_capacity(void);

/* Get sector size in bytes (usually 512) */
uint32_t virtio_blk_sector_size(void);

/* Check if block device is available */
int virtio_blk_available(void);

#endif /* VIRTIO_BLK_H */
