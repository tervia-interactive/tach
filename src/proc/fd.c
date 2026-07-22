#include <kernel/types.h>
#include <proc/fd.h>

int fd_alloc(struct fd_table* table, struct vnode* vnode, int flags) {
    (void)table;
    (void)vnode;
    (void)flags;
    return 0;
}

void fd_free(int fd) {
    (void)fd;
}

struct vnode* fd_get_vnode(struct fd_table* table, int fd) {
    (void)table;
    (void)fd;
    return NULL;
}

int fd_dup(struct fd_table* table, int oldfd, int newfd) {
    (void)table;
    (void)oldfd;
    (void)newfd;
    return 0;
}
