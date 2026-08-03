#include <kernel/errno.h>
#include <kernel/string.h>
#include <fs/vfs.h>
#include <hal/console.h>

#define VFS_MAX_MEMFILES 64

static struct vnode g_root;
static struct vnode g_console;
static struct vnode g_memfiles[VFS_MAX_MEMFILES];
static size_t g_memfile_count;

static int simple_open(struct vnode* vnode, int flags) {
    (void)vnode; (void)flags;
    return 0;
}
static int simple_close(struct vnode* vnode) { (void)vnode; return 0; }

static ssize_t console_read(struct vnode* vnode, void* buffer, size_t count,
                            int64_t offset) {
    (void)vnode; (void)offset;
    if (!buffer) return -EFAULT;
    char* output = (char*)buffer;
    for (size_t i = 0; i < count; i++) output[i] = hal_console_getchar();
    return (ssize_t)count;
}

static ssize_t console_write(struct vnode* vnode, const void* buffer,
                             size_t count, int64_t offset) {
    (void)vnode; (void)offset;
    if (!buffer) return -EFAULT;
    hal_console_write((const char*)buffer, count);
    return (ssize_t)count;
}

static ssize_t memfile_read(struct vnode* vnode, void* buffer, size_t count,
                            int64_t offset) {
    if (!vnode || !buffer || offset < 0) return -EFAULT;
    if ((size_t)offset >= vnode->size) return 0;
    size_t available = vnode->size - (size_t)offset;
    if (count > available) count = available;
    memcpy(buffer, (const uint8_t*)vnode->data + (size_t)offset, count);
    return (ssize_t)count;
}

static struct vnode_ops g_console_ops = {
    .open = simple_open, .close = simple_close,
    .read = console_read, .write = console_write, .readdir = NULL
};
static struct vnode_ops g_memfile_ops = {
    .open = simple_open, .close = simple_close,
    .read = memfile_read, .write = NULL, .readdir = NULL
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
    memset(g_memfiles, 0, sizeof(g_memfiles));
    g_memfile_count = 0;
}

struct vnode* vfs_root(void) { return &g_root; }
struct vnode* vfs_console(void) { return &g_console; }

int vfs_mount(const char* path, struct vnode* root) {
    if (!path || !root) return -EINVAL;
    if (strcmp(path, "/") != 0) return -ENOTSUP;
    g_root = *root;
    return 0;
}

struct vnode* vfs_open(const char* path, int flags) {
    if (!path) return NULL;
    struct vnode* vnode = NULL;
    if (strcmp(path, "/") == 0) vnode = &g_root;
    else if (strcmp(path, "/dev/console") == 0 ||
             strcmp(path, "/dev/tty") == 0) vnode = &g_console;
    else {
        for (size_t i = 0; i < g_memfile_count; i++) {
            if (strcmp(path, g_memfiles[i].path) == 0) {
                vnode = &g_memfiles[i];
                break;
            }
        }
    }
    if (!vnode) return NULL;
    if (vnode->ops && vnode->ops->open && vnode->ops->open(vnode, flags) < 0)
        return NULL;
    vnode->refcount++;
    return vnode;
}

int vfs_close(struct vnode* vnode) {
    if (!vnode) return -EBADF;
    int result = 0;
    if (vnode->ops && vnode->ops->close) result = vnode->ops->close(vnode);
    if (vnode->refcount) vnode->refcount--;
    return result;
}

ssize_t vfs_read(struct vnode* vnode, void* buffer, size_t count,
                 int64_t offset) {
    if (!vnode || !vnode->ops || !vnode->ops->read) return -EBADF;
    return vnode->ops->read(vnode, buffer, count, offset);
}

ssize_t vfs_write(struct vnode* vnode, const void* buffer, size_t count,
                  int64_t offset) {
    if (!vnode || !vnode->ops || !vnode->ops->write) return -EBADF;
    return vnode->ops->write(vnode, buffer, count, offset);
}

int vfs_register_memfile(const char* path, const void* data, size_t size) {
    if (!path || path[0] != '/' || !data || !size) return -EINVAL;
    if (strlen(path) >= sizeof(g_memfiles[0].path)) return -ENAMETOOLONG;
    for (size_t i = 0; i < g_memfile_count; i++)
        if (strcmp(path, g_memfiles[i].path) == 0) return -EEXIST;
    if (g_memfile_count >= VFS_MAX_MEMFILES) return -ENOSPC;

    struct vnode* vnode = &g_memfiles[g_memfile_count++];
    memset(vnode, 0, sizeof(*vnode));
    strncpy(vnode->path, path, sizeof(vnode->path) - 1);
    char* base = vnode->path;
    for (char* cursor = vnode->path; *cursor; cursor++)
        if (*cursor == '/') base = cursor + 1;
    vnode->name = base;
    vnode->type = VNODE_FILE;
    vnode->parent = &g_root;
    vnode->data = (void*)data;
    vnode->size = size;
    vnode->ops = &g_memfile_ops;
    vnode->refcount = 1;
    return 0;
}

int vfs_read_file(const char* path, const void** data, size_t* size) {
    if (!path || !data || !size) return -EINVAL;
    for (size_t i = 0; i < g_memfile_count; i++) {
        if (strcmp(path, g_memfiles[i].path) != 0) continue;
        *data = g_memfiles[i].data;
        *size = g_memfiles[i].size;
        return 0;
    }
    return -ENOENT;
}
