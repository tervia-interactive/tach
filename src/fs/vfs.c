#include <kernel/errno.h>
#include <kernel/string.h>
#include <fs/vfs.h>
#include <hal/console.h>

#define VFS_MAX_MEMNODES 96
#define VFS_MAX_MOUNTS 8

struct vfs_mount_entry {
    bool used;
    char path[128];
    struct vnode* root;
};

static struct vnode g_root;
static struct vnode g_console;
static struct vnode g_memnodes[VFS_MAX_MEMNODES];
static size_t g_memnode_count;
static struct vfs_mount_entry g_mounts[VFS_MAX_MOUNTS];

static int simple_open(struct vnode* vnode, int flags) {
    if ((flags & O_TRUNC) && vnode && vnode->type == VNODE_FILE) {
        if (!vnode->ops || !vnode->ops->truncate ||
            vnode->ops->truncate(vnode, 0) < 0) return -EROFS;
    }
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

static struct vnode* virtual_lookup(struct vnode* directory,
                                    const char* name) {
    for (size_t i = 0; i < g_memnode_count; i++) {
        if (g_memnodes[i].parent == directory &&
            strcmp(g_memnodes[i].name, name) == 0) return &g_memnodes[i];
    }
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].used || g_mounts[i].root->parent != directory)
            continue;
        if (strcmp(g_mounts[i].root->name, name) == 0)
            return g_mounts[i].root;
    }
    return NULL;
}

static int virtual_readdir(struct vnode* directory, struct dirent* entry,
                           int64_t offset) {
    if (!directory || !entry || offset < 0) return -EINVAL;
    int64_t current = 0;
    for (size_t i = 0; i < g_memnode_count; i++) {
        if (g_memnodes[i].parent != directory) continue;
        if (current++ != offset) continue;
        memset(entry, 0, sizeof(*entry));
        entry->d_ino = (uint32_t)(i + 1);
        entry->d_off = (uint32_t)(offset + 1);
        entry->d_type = (uint8_t)g_memnodes[i].type;
        strncpy(entry->d_name, g_memnodes[i].name, sizeof(entry->d_name) - 1);
        entry->d_reclen = sizeof(*entry);
        return 1;
    }
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].used || g_mounts[i].root->parent != directory)
            continue;
        if (current++ != offset) continue;
        memset(entry, 0, sizeof(*entry));
        entry->d_ino = (uint32_t)(VFS_MAX_MEMNODES + i + 1);
        entry->d_off = (uint32_t)(offset + 1);
        entry->d_type = VNODE_DIR;
        strncpy(entry->d_name, g_mounts[i].root->name,
                sizeof(entry->d_name) - 1);
        entry->d_reclen = sizeof(*entry);
        return 1;
    }
    return 0;
}

static struct vnode_ops g_console_ops = {
    .open = simple_open, .close = simple_close,
    .read = console_read, .write = console_write
};
static struct vnode_ops g_memfile_ops = {
    .open = simple_open, .close = simple_close, .read = memfile_read
};
static struct vnode_ops g_virtual_dir_ops = {
    .open = simple_open, .close = simple_close,
    .readdir = virtual_readdir, .lookup = virtual_lookup
};

static struct vnode* allocate_memnode(struct vnode* parent, const char* name,
                                      int type) {
    if (g_memnode_count >= VFS_MAX_MEMNODES || !name || !*name ||
        strlen(name) >= sizeof(g_memnodes[0].name_storage)) return NULL;
    struct vnode* node = &g_memnodes[g_memnode_count++];
    memset(node, 0, sizeof(*node));
    strncpy(node->name_storage, name, sizeof(node->name_storage) - 1);
    node->name = node->name_storage;
    node->parent = parent;
    node->type = type;
    node->refcount = 1;
    node->ops = type == VNODE_DIR ? &g_virtual_dir_ops : &g_memfile_ops;
    return node;
}

void vfs_init(void) {
    memset(&g_root, 0, sizeof(g_root));
    g_root.type = VNODE_DIR;
    strcpy(g_root.name_storage, "/");
    g_root.name = g_root.name_storage;
    g_root.ops = &g_virtual_dir_ops;
    g_root.refcount = 1;
    memset(&g_console, 0, sizeof(g_console));
    g_console.type = VNODE_CHAR;
    strcpy(g_console.name_storage, "console");
    g_console.name = g_console.name_storage;
    g_console.parent = &g_root;
    g_console.ops = &g_console_ops;
    g_console.refcount = 1;
    memset(g_memnodes, 0, sizeof(g_memnodes));
    memset(g_mounts, 0, sizeof(g_mounts));
    g_memnode_count = 0;
}

struct vnode* vfs_root(void) { return &g_root; }
struct vnode* vfs_console(void) { return &g_console; }

static struct vnode* resolve_from(struct vnode* node, const char* relative) {
    char component[64];
    const char* cursor = relative;
    while (*cursor == '/') cursor++;
    while (*cursor) {
        size_t length = 0;
        while (cursor[length] && cursor[length] != '/') length++;
        if (!length) { cursor++; continue; }
        if (length >= sizeof(component) || !node || node->type != VNODE_DIR ||
            !node->ops || !node->ops->lookup) return NULL;
        memcpy(component, cursor, length);
        component[length] = '\0';
        if (strcmp(component, ".") == 0) {
        } else if (strcmp(component, "..") == 0) {
            if (node->parent) node = node->parent;
        } else {
            node = node->ops->lookup(node, component);
        }
        if (!node) return NULL;
        cursor += length;
        while (*cursor == '/') cursor++;
    }
    return node;
}

static struct vfs_mount_entry* mount_for_path(const char* path) {
    struct vfs_mount_entry* best = NULL;
    size_t best_length = 0;
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!g_mounts[i].used) continue;
        size_t length = strlen(g_mounts[i].path);
        if (length < best_length || strncmp(path, g_mounts[i].path, length))
            continue;
        if (path[length] && path[length] != '/') continue;
        best = &g_mounts[i];
        best_length = length;
    }
    return best;
}

static struct vnode* resolve_path(const char* path) {
    if (!path || path[0] != '/') return NULL;
    if (strcmp(path, "/dev/console") == 0 || strcmp(path, "/dev/tty") == 0)
        return &g_console;
    struct vfs_mount_entry* mount = mount_for_path(path);
    if (mount) return resolve_from(mount->root, path + strlen(mount->path));
    return resolve_from(&g_root, path + 1);
}

static int split_parent(const char* path, char* parent, size_t parent_size,
                        char* name, size_t name_size) {
    if (!path || path[0] != '/' || !path[1]) return -EINVAL;
    size_t length = strlen(path);
    while (length > 1 && path[length - 1] == '/') length--;
    size_t slash = length;
    while (slash && path[slash - 1] != '/') slash--;
    size_t name_length = length - slash;
    if (!name_length || name_length >= name_size || slash >= parent_size)
        return -ENAMETOOLONG;
    memcpy(name, path + slash, name_length);
    name[name_length] = '\0';
    if (slash <= 1) strcpy(parent, "/");
    else { memcpy(parent, path, slash - 1); parent[slash - 1] = '\0'; }
    return 0;
}

int vfs_mount(const char* path, struct vnode* root) {
    if (!path || !root || path[0] != '/' || strlen(path) >= 128)
        return -EINVAL;
    if (strcmp(path, "/") == 0) { g_root = *root; return 0; }
    char parent_path[128], name[64];
    int split = split_parent(path, parent_path, sizeof(parent_path),
                             name, sizeof(name));
    if (split < 0) return split;
    struct vnode* parent = resolve_path(parent_path);
    if (!parent || parent->type != VNODE_DIR) return -ENOENT;
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].used) continue;
        g_mounts[i].used = true;
        strcpy(g_mounts[i].path, path);
        g_mounts[i].root = root;
        root->parent = parent;
        strncpy(root->name_storage, name, sizeof(root->name_storage) - 1);
        root->name = root->name_storage;
        return 0;
    }
    return -ENOSPC;
}

struct vnode* vfs_open(const char* path, int flags) {
    struct vnode* vnode = resolve_path(path);
    if (!vnode && (flags & O_CREAT)) {
        char parent_path[128], name[64];
        if (split_parent(path, parent_path, sizeof(parent_path), name,
                         sizeof(name)) < 0) return NULL;
        struct vnode* parent = resolve_path(parent_path);
        if (!parent || !parent->ops || !parent->ops->create) return NULL;
        vnode = parent->ops->create(parent, name, VNODE_FILE);
    }
    if (!vnode) return NULL;
    if (vnode->type == VNODE_DIR && (flags & O_ACCMODE) != O_RDONLY)
        return NULL;
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

int vfs_readdir(struct vnode* vnode, struct dirent* entry, int64_t offset) {
    if (!vnode || vnode->type != VNODE_DIR) return -ENOTDIR;
    if (!vnode->ops || !vnode->ops->readdir) return -ENOTSUP;
    return vnode->ops->readdir(vnode, entry, offset);
}

int vfs_mkdir(const char* path) {
    if (resolve_path(path)) return -EEXIST;
    char parent_path[128], name[64];
    int result = split_parent(path, parent_path, sizeof(parent_path), name,
                              sizeof(name));
    if (result < 0) return result;
    struct vnode* parent = resolve_path(parent_path);
    if (!parent) return -ENOENT;
    if (!parent->ops || !parent->ops->create) return -EROFS;
    return parent->ops->create(parent, name, VNODE_DIR) ? 0 : -EIO;
}

int vfs_unlink(const char* path) {
    char parent_path[128], name[64];
    int result = split_parent(path, parent_path, sizeof(parent_path), name,
                              sizeof(name));
    if (result < 0) return result;
    struct vnode* parent = resolve_path(parent_path);
    if (!parent) return -ENOENT;
    if (!parent->ops || !parent->ops->unlink) return -EROFS;
    return parent->ops->unlink(parent, name);
}

int vfs_sync(void) {
    int result = 0;
    for (size_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].used && g_mounts[i].root->ops &&
            g_mounts[i].root->ops->sync) {
            int one = g_mounts[i].root->ops->sync(g_mounts[i].root);
            if (one < 0) result = one;
        }
    }
    return result;
}

int vfs_register_memfile(const char* path, const void* data, size_t size) {
    if (!path || path[0] != '/' || !data || !size) return -EINVAL;
    if (strlen(path) >= sizeof(g_memnodes[0].path)) return -ENAMETOOLONG;
    if (resolve_path(path)) return -EEXIST;
    struct vnode* parent = &g_root;
    const char* cursor = path + 1;
    char component[64];
    while (*cursor) {
        size_t length = 0;
        while (cursor[length] && cursor[length] != '/') length++;
        if (!length || length >= sizeof(component)) return -ENAMETOOLONG;
        memcpy(component, cursor, length); component[length] = '\0';
        cursor += length;
        while (*cursor == '/') cursor++;
        struct vnode* node = virtual_lookup(parent, component);
        if (!node) node = allocate_memnode(parent, component,
                                            *cursor ? VNODE_DIR : VNODE_FILE);
        if (!node || (*cursor && node->type != VNODE_DIR)) return -ENOSPC;
        parent = node;
    }
    parent->data = (void*)data;
    parent->size = size;
    strcpy(parent->path, path);
    return 0;
}

int vfs_read_file(const char* path, const void** data, size_t* size) {
    if (!path || !data || !size) return -EINVAL;
    struct vnode* vnode = resolve_path(path);
    if (!vnode || vnode->type != VNODE_FILE) return -ENOENT;
    if (vnode->ops != &g_memfile_ops) return -ENOTSUP;
    *data = vnode->data;
    *size = vnode->size;
    return 0;
}
