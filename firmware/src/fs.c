/*
 * fs.c - Filesystem API wrapper for lwext4
 *
 * Provides a simple file I/O interface for the web server.
 */

#include "fs.h"
#include "ext4_blockdev_virtio.h"
#include "console.h"

#include <string.h>

/* Include our config first */
#define CONFIG_USE_DEFAULT_CFG 0

#include <ext4.h>
#include <ext4_errno.h>

/* Mount point */
#define MOUNT_POINT "/"

/* File handle table */
static struct {
    ext4_file file;
    int in_use;
} file_table[FS_MAX_OPEN_FILES];

/* Filesystem state */
static int fs_is_mounted = 0;

/* Find a free file handle slot */
static int find_free_slot(void)
{
    for (int i = 0; i < FS_MAX_OPEN_FILES; i++) {
        if (!file_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

int fs_init(void)
{
    int r;

    if (fs_is_mounted) {
        return 0;  /* Already mounted */
    }

    console_printf("fs: Initializing filesystem...\n");

    /* Clear file table */
    memset(file_table, 0, sizeof(file_table));

    /* Get block device */
    struct ext4_blockdev *bd = ext4_blockdev_virtio_get();
    if (bd == NULL) {
        console_printf("fs: Failed to get block device\n");
        return -1;
    }

    /* Register block device */
    r = ext4_device_register(bd, ext4_blockdev_virtio_name());
    if (r != EOK) {
        console_printf("fs: Failed to register block device: %d\n", r);
        return -1;
    }

    /* Mount filesystem */
    r = ext4_mount(ext4_blockdev_virtio_name(), MOUNT_POINT, false);
    if (r != EOK) {
        console_printf("fs: Failed to mount filesystem: %d\n", r);
        ext4_device_unregister(ext4_blockdev_virtio_name());
        return -1;
    }

    fs_is_mounted = 1;
    console_printf("fs: Filesystem mounted successfully\n");
    return 0;
}

void fs_shutdown(void)
{
    if (!fs_is_mounted) {
        return;
    }

    /* Close all open files */
    for (int i = 0; i < FS_MAX_OPEN_FILES; i++) {
        if (file_table[i].in_use) {
            ext4_fclose(&file_table[i].file);
            file_table[i].in_use = 0;
        }
    }

    /* Flush cache */
    ext4_cache_flush(MOUNT_POINT);

    /* Unmount */
    ext4_umount(MOUNT_POINT);
    ext4_device_unregister(ext4_blockdev_virtio_name());

    fs_is_mounted = 0;
    console_printf("fs: Filesystem unmounted\n");
}

fs_file_t fs_open(const char *path, int flags)
{
    if (!fs_is_mounted || path == NULL) {
        return FS_INVALID_FILE;
    }

    int slot = find_free_slot();
    if (slot < 0) {
        console_printf("fs: No free file handles\n");
        return FS_INVALID_FILE;
    }

    /* Build flags string for ext4_fopen */
    const char *mode;
    if ((flags & FS_O_RDWR) == FS_O_RDWR) {
        if (flags & FS_O_CREAT) {
            if (flags & FS_O_TRUNC) {
                mode = "w+";
            } else if (flags & FS_O_APPEND) {
                mode = "a+";
            } else {
                mode = "r+";
            }
        } else {
            mode = "r+";
        }
    } else if (flags & FS_O_WRONLY) {
        if (flags & FS_O_APPEND) {
            mode = "a";
        } else if (flags & FS_O_TRUNC) {
            mode = "w";
        } else {
            mode = "w";
        }
    } else {
        /* Read-only */
        mode = "r";
    }

    int r = ext4_fopen(&file_table[slot].file, path, mode);
    if (r != EOK) {
        console_printf("fs: Failed to open %s: %d\n", path, r);
        return FS_INVALID_FILE;
    }

    file_table[slot].in_use = 1;
    return slot;
}

int fs_close(fs_file_t fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        return -1;
    }

    int r = ext4_fclose(&file_table[fd].file);
    file_table[fd].in_use = 0;

    return (r == EOK) ? 0 : -1;
}

ssize_t fs_read(fs_file_t fd, void *buf, size_t size)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        return -1;
    }

    size_t rcnt = 0;
    int r = ext4_fread(&file_table[fd].file, buf, size, &rcnt);
    if (r != EOK) {
        return -1;
    }

    return (ssize_t)rcnt;
}

ssize_t fs_write(fs_file_t fd, const void *buf, size_t size)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        return -1;
    }

    size_t wcnt = 0;
    int r = ext4_fwrite(&file_table[fd].file, buf, size, &wcnt);
    if (r != EOK) {
        return -1;
    }

    return (ssize_t)wcnt;
}

int64_t fs_seek(fs_file_t fd, int64_t offset, int whence)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        return -1;
    }

    uint32_t origin;
    switch (whence) {
    case FS_SEEK_SET:
        origin = SEEK_SET;
        break;
    case FS_SEEK_CUR:
        origin = SEEK_CUR;
        break;
    case FS_SEEK_END:
        origin = SEEK_END;
        break;
    default:
        return -1;
    }

    int r = ext4_fseek(&file_table[fd].file, offset, origin);
    if (r != EOK) {
        return -1;
    }

    return (int64_t)ext4_ftell(&file_table[fd].file);
}

int64_t fs_tell(fs_file_t fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        return -1;
    }

    return (int64_t)ext4_ftell(&file_table[fd].file);
}

int64_t fs_size(fs_file_t fd)
{
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !file_table[fd].in_use) {
        return -1;
    }

    return (int64_t)ext4_fsize(&file_table[fd].file);
}

int fs_exists(const char *path)
{
    if (!fs_is_mounted || path == NULL) {
        return 0;
    }

    ext4_file f;
    int r = ext4_fopen(&f, path, "r");
    if (r == EOK) {
        ext4_fclose(&f);
        return 1;
    }

    return 0;
}

int64_t fs_stat_size(const char *path)
{
    if (!fs_is_mounted || path == NULL) {
        return -1;
    }

    ext4_file f;
    int r = ext4_fopen(&f, path, "r");
    if (r != EOK) {
        return -1;
    }

    int64_t size = (int64_t)ext4_fsize(&f);
    ext4_fclose(&f);

    return size;
}

int fs_mkdir(const char *path)
{
    if (!fs_is_mounted || path == NULL) {
        return -1;
    }

    int r = ext4_dir_mk(path);
    return (r == EOK) ? 0 : -1;
}

int fs_mounted(void)
{
    return fs_is_mounted;
}
