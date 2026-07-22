#include <kernel/types.h>
#include <fs/vfs.h>
void vfs_init(void) { }
struct vnode* vfs_root(void) { return (struct vnode*)0; }
int vfs_mount(const char *path, struct vnode *root) {(void)path;(void)root; return 0;}
struct vnode* vfs_open(const char *path, int flags) {(void)path;(void)flags; return (struct vnode*)0;}
int vfs_close(struct vnode *vn) {(void)vn; return 0;}
ssize_t vfs_read(struct vnode *vn, void *buf, size_t count, int64_t offset) {(void)vn;(void)buf;(void)count;(void)offset; return 0;}
ssize_t vfs_write(struct vnode *vn, const void *buf, size_t count, int64_t offset) {(void)vn;(void)buf;(void)count;(void)offset; return 0;}
