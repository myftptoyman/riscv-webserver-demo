/*
 * fs.h - Filesystem API wrapper for lwext4
 *
 * Provides a simple file I/O interface for the web server.
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>

/* File open flags */
#define FS_O_RDONLY     0x00
#define FS_O_WRONLY     0x01
#define FS_O_RDWR       0x02
#define FS_O_CREAT      0x0100
#define FS_O_TRUNC      0x1000
#define FS_O_APPEND     0x2000

/* Seek origins */
#define FS_SEEK_SET     0
#define FS_SEEK_CUR     1
#define FS_SEEK_END     2

/* Maximum number of open files */
#define FS_MAX_OPEN_FILES 8

/* Maximum path length */
#define FS_MAX_PATH 256

/* File handle (opaque) */
typedef int fs_file_t;

/* Invalid file handle */
#define FS_INVALID_FILE (-1)

/* Initialize the filesystem
 * Mounts the ext4 filesystem from the VirtIO block device
 * Returns: 0 on success, negative on error
 */
int fs_init(void);

/* Shutdown the filesystem
 * Unmounts and flushes all data
 */
void fs_shutdown(void);

/* Open a file
 * path: File path (must start with /)
 * flags: Open flags (FS_O_*)
 * Returns: File handle on success, FS_INVALID_FILE on error
 */
fs_file_t fs_open(const char *path, int flags);

/* Close a file
 * fd: File handle from fs_open
 * Returns: 0 on success, negative on error
 */
int fs_close(fs_file_t fd);

/* Read from a file
 * fd: File handle
 * buf: Buffer to read into
 * size: Number of bytes to read
 * Returns: Number of bytes read, 0 on EOF, negative on error
 */
ssize_t fs_read(fs_file_t fd, void *buf, size_t size);

/* Write to a file
 * fd: File handle
 * buf: Data to write
 * size: Number of bytes to write
 * Returns: Number of bytes written, negative on error
 */
ssize_t fs_write(fs_file_t fd, const void *buf, size_t size);

/* Seek in a file
 * fd: File handle
 * offset: Seek offset
 * whence: Seek origin (FS_SEEK_*)
 * Returns: New file position, negative on error
 */
int64_t fs_seek(fs_file_t fd, int64_t offset, int whence);

/* Get current file position
 * fd: File handle
 * Returns: Current position, negative on error
 */
int64_t fs_tell(fs_file_t fd);

/* Get file size
 * fd: File handle
 * Returns: File size in bytes, negative on error
 */
int64_t fs_size(fs_file_t fd);

/* Check if a file exists
 * path: File path
 * Returns: 1 if exists, 0 if not
 */
int fs_exists(const char *path);

/* Get file size by path (without opening)
 * path: File path
 * Returns: File size in bytes, negative on error
 */
int64_t fs_stat_size(const char *path);

/* Create a directory
 * path: Directory path
 * Returns: 0 on success, negative on error
 */
int fs_mkdir(const char *path);

/* Check if filesystem is mounted */
int fs_mounted(void);

#endif /* FS_H */
