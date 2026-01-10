/*
 * ext4_config.h - lwext4 configuration for bare-metal RISC-V
 */

#ifndef MY_EXT4_CONFIG_H_
#define MY_EXT4_CONFIG_H_

/* Use ext4 feature set */
#define CONFIG_EXT_FEATURE_SET_LVL 4

/* Disable journaling to save memory */
#define CONFIG_JOURNALING_ENABLE 0

/* Enable extents support */
#define CONFIG_EXTENTS_ENABLE 1

/* Enable xattr (required by core ext4 functions) */
#define CONFIG_XATTR_ENABLE 1

/* Use our own errno definitions */
#define CONFIG_HAVE_OWN_ERRNO 1

/* Enable debug output */
#define CONFIG_DEBUG_PRINTF 1
#define CONFIG_DEBUG_ASSERT 1

/* Use our own assert */
#define CONFIG_HAVE_OWN_ASSERT 1

/* Disable block device statistics to save memory */
#define CONFIG_BLOCK_DEV_ENABLE_STATS 0

/* Small cache size for limited memory */
#define CONFIG_BLOCK_DEV_CACHE_SIZE 8

/* Single block device */
#define CONFIG_EXT4_BLOCKDEVS_COUNT 1

/* Single mount point */
#define CONFIG_EXT4_MOUNTPOINTS_COUNT 1

/* Maximum block device name length */
#define CONFIG_EXT4_MAX_BLOCKDEV_NAME 16

/* Maximum mount point name length */
#define CONFIG_EXT4_MAX_MP_NAME 16

/* Use our own open flags */
#define CONFIG_HAVE_OWN_OFLAGS 1

/* Limit truncate size */
#define CONFIG_MAX_TRUNCATE_SIZE (4ul * 1024ul * 1024ul)

/* Disable unaligned access */
#define CONFIG_UNALIGNED_ACCESS 0

/* Use our own malloc */
#define CONFIG_USE_USER_MALLOC 1

#endif /* MY_EXT4_CONFIG_H_ */
