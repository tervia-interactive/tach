#include <kernel/types.h>
#include <fs/vfs.h>
void vfs_init(void) { }
int vfs_open(const char *path) {(void)path; return 0;}
int vfs_read(int fd, void *buf, size_t len) {(void)fd;(void)buf;(void)len; return 0;}
int vfs_write(int fd, const void *buf, size_t len) {(void)fd;(void)buf;(void)len; return 0;}
int vfs_close(int fd) {(void)fd; return 0;}
