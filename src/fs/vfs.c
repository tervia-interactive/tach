#include <kernel/errno.h>
#include <kernel/string.h>
#include <fs/vfs.h>
#include <hal/console.h>

static struct vnode g_root;
static struct vnode g_console;

static int console_open(struct vnode* vn, int flags) {
    (void)vn;
    (void)flags;
    return 0;
}

static int console_close(struct vnode* vn) {
    (void)vn;
    return 0;
}

static ssize_t console_read(struct vnode* vn, void* buf, size_t count,
                            int64_t offset) {
    (void)vn;
    (void)offset;
    if (!buf) {
        return -EFAULT;
    }
    char* out = (char*)buf;
    for (size_t i = 0; i < count; i++) {
        out[i] = hal_console_getchar();
    }
    return (ssize_t)count;
}

static ssize_t console_write(struct vnode* vn, const void* buf, size_t count,
                             int64_t offset) {
    (void)vn;
    (void)offset;
    if (!buf) {
        return -EFAULT;
    }
    hal_console_write((const char*)buf, count);
    return (ssize_t)count;
}

static struct vnode_ops g_console_ops = {
    .open = console_open,
    .close = console_close,
    .read = console_read,
    .write = console_write,
    .readdir = NULL
};

void vfs_init(void) {
    memset(&g_root, 0, sizeof(g_root));
    g_root.type = VNODE_DIR;
    g_root.name = "/";
    g_root.refcount = 1;

    memset(&g_console, 0, sizeof(g_console));
    g_console.type = VNODE_CHAR;
    g_console.name = "console";
    g_console.parent = &g_root;
    g_console.ops = &g_console_ops;
    g_console.refcount = 1;
}

struct vnode* vfs_root(void) {
    return &g_root;
}

struct vnode* vfs_console(void) {
    return &g_console;
}

int vfs_mount(const char *path, struct vnode *root) {
    if (!path || !root) {
        return -EINVAL;
    }
    if (strcmp(path, "/") == 0) {
        g_root = *root;
        return 0;
    }
    return -ENOTSUP;
}

struct vnode* vfs_open(const char *path, int flags) {
    if (!path) {
        return NULL;
    }
    struct vnode* vnode = NULL;
    if (strcmp(path, "/") == 0) {
        vnode = &g_root;
    } else if (strcmp(path, "/dev/console") == 0 ||
               strcmp(path, "/dev/tty") == 0) {
        vnode = &g_console;
    }
    if (!vnode) {
        return NULL;
    }
    if (vnode->ops && vnode->ops->open &&
        vnode->ops->open(vnode, flags) < 0) {
        return NULL;
    }
    vnode->refcount++;
    return vnode;
}

int vfs_close(struct vnode *vn) {
    if (!vn) {
        return -EBADF;
    }
    int result = 0;
    if (vn->ops && vn->ops->close) {
        result = vn->ops->close(vn);
    }
    if (vn->refcount) {
        vn->refcount--;
    }
    return result;
}

ssize_t vfs_read(struct vnode *vn, void *buf, size_t count, int64_t offset) {
    if (!vn || !vn->ops || !vn->ops->read) {
        return -EBADF;
    }
    return vn->ops->read(vn, buf, count, offset);
}

ssize_t vfs_write(struct vnode *vn, const void *buf, size_t count,
                  int64_t offset) {
    if (!vn || !vn->ops || !vn->ops->write) {
        return -EBADF;
    }
    return vn->ops->write(vn, buf, count, offset);
}
