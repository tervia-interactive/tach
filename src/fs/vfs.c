#include <kernel/errno.h>
#include <kernel/string.h>
#include <fs/vfs.h>
#include <fs/procfs.h>
#include <fs/tmpfs.h>
#include <hal/console.h>

#define VFS_MAX_MEMFILES 64

static struct vnode g_root;
static struct vnode g_console;
static struct vnode g_memfiles[VFS_MAX_MEMFILES];
static size_t g_memfile_count;

/* Fallback fraco: binarios sem procfs.c (ex.: host test) veem /proc vazio.
 * O procfs.c do kernel define a versao forte. */
__attribute__((weak)) struct vnode* procfs_lookup(const char* path) {
    (void)path;
    return NULL;
}

/* Fallbacks fracos para binarios sem tmpfs.c (host test); o do kernel
 * define as versoes fortes. */
__attribute__((weak)) int tmpfs_owns(const char* path) {
    (void)path;
    return 0;
}
__attribute__((weak)) struct vnode* tmpfs_lookup(const char* path, int flags,
                                                 int* error) {
    (void)path; (void)flags;
    if (error) *error = -ENOENT;
    return NULL;
}
__attribute__((weak)) int tmpfs_unlink(const char* path) {
    (void)path;
    return -ENOENT;
}

__attribute__((weak)) int tmpfs_list(size_t index, struct dirent* entry) {
    (void)index; (void)entry;
    return 0;
}

/* Sem procfs.c (host test), /proc nao tem arquivos. */
__attribute__((weak)) size_t procfs_count(void) { return 0; }
__attribute__((weak)) const char* procfs_path(size_t index) {
    (void)index;
    return NULL;
}

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

/* ------------------------------------------------------------ diretorios
 *
 * Nao ha uma arvore de inodes: os diretorios sao derivados dos caminhos que
 * existem (arquivos do initrd, /proc, dispositivos fixos). "/bin" e um
 * diretorio porque "/bin/sh" existe. Cada diretorio aberto ganha um vnode
 * persistente num pequeno pool, indexado pelo caminho. */

#define VFS_MAX_DIRS 16
#define VFS_PREFIX_MAX (sizeof(g_memfiles[0].path) + 2)

static struct vnode g_dirs[VFS_MAX_DIRS];

static int dir_open(struct vnode* vnode, int flags) {
    (void)vnode;
    return (flags & O_ACCMODE) == O_RDONLY ? 0 : -EISDIR;
}
static ssize_t dir_read(struct vnode* vnode, void* buffer, size_t count,
                        int64_t offset) {
    (void)vnode; (void)buffer; (void)count; (void)offset;
    return -EISDIR; /* use getdents */
}
static struct vnode_ops g_dir_ops = {
    .open = dir_open, .close = simple_close,
    .read = dir_read, .write = NULL, .readdir = NULL
};

/* Entradas que nao vem de memfile nem de procfs/tmpfs. */
struct vfs_fixed_entry {
    const char* path;
    int type;
};
static const struct vfs_fixed_entry g_fixed[] = {
    {"/dev/console", VNODE_CHAR},
    {"/dev/tty", VNODE_CHAR},
    {"/tmp", VNODE_DIR},
};
#define VFS_FIXED_COUNT (sizeof(g_fixed) / sizeof(g_fixed[0]))

static size_t candidate_total(void) {
    return VFS_FIXED_COUNT + g_memfile_count + procfs_count();
}

/* i-esimo caminho conhecido (fixos, depois initrd, depois /proc), na mesma
 * ordem sempre. false se o indice nao tem caminho. */
static bool candidate_at(size_t index, const char** path, int* type) {
    if (index < VFS_FIXED_COUNT) {
        *path = g_fixed[index].path;
        *type = g_fixed[index].type;
        return true;
    }
    index -= VFS_FIXED_COUNT;
    if (index < g_memfile_count) {
        *path = g_memfiles[index].path;
        *type = VNODE_FILE;
        return true;
    }
    index -= g_memfile_count;
    if (index < procfs_count()) {
        *path = procfs_path(index);
        *type = VNODE_FILE;
        return *path != NULL;
    }
    return false;
}

/* Monta o prefixo "dir/" ("/" para a raiz). false se nao couber. */
static bool dir_prefix(const char* dir, char* prefix, size_t* length) {
    size_t n = strlen(dir);
    if (n + 2 > VFS_PREFIX_MAX) return false;
    memcpy(prefix, dir, n);
    if (n != 1) prefix[n++] = '/';
    prefix[n] = '\0';
    *length = n;
    return true;
}

/* Se `path` esta abaixo do diretorio de prefixo `prefix`, devolve o tamanho
 * do primeiro componente e se ha mais componentes depois dele. */
static bool child_of(const char* path, const char* prefix, size_t prefix_length,
                     size_t* name_length, bool* nested) {
    if (strncmp(path, prefix, prefix_length) != 0) return false;
    const char* name = path + prefix_length;
    size_t length = 0;
    while (name[length] && name[length] != '/') length++;
    if (!length) return false;
    *name_length = length;
    *nested = name[length] == '/';
    return true;
}

static bool is_directory(const char* path) {
    if (strcmp(path, "/") == 0) return true;
    size_t n = strlen(path);
    if (n == 0 || path[n - 1] == '/') return false;
    char prefix[VFS_PREFIX_MAX];
    size_t prefix_length;
    if (!dir_prefix(path, prefix, &prefix_length)) return false;
    size_t total = candidate_total();
    for (size_t i = 0; i < total; i++) {
        const char* candidate;
        int type;
        size_t length;
        bool nested;
        if (candidate_at(i, &candidate, &type) &&
            child_of(candidate, prefix, prefix_length, &length, &nested))
            return true;
    }
    return false;
}

static struct vnode* dir_vnode(const char* path, int* error) {
    if (strlen(path) >= sizeof(g_dirs[0].path)) {
        if (error) *error = -ENAMETOOLONG;
        return NULL;
    }
    for (size_t i = 0; i < VFS_MAX_DIRS; i++)
        if (g_dirs[i].path[0] && strcmp(g_dirs[i].path, path) == 0)
            return &g_dirs[i];
    for (size_t i = 0; i < VFS_MAX_DIRS; i++) {
        struct vnode* vnode = &g_dirs[i];
        if (vnode->path[0]) continue;
        memset(vnode, 0, sizeof(*vnode));
        strncpy(vnode->path, path, sizeof(vnode->path) - 1);
        const char* base = vnode->path;
        for (const char* cursor = vnode->path; *cursor; cursor++)
            if (*cursor == '/') base = cursor + 1;
        vnode->name = (char*)base;
        vnode->type = VNODE_DIR;
        vnode->parent = &g_root;
        vnode->ops = &g_dir_ops;
        vnode->refcount = 1;
        return vnode;
    }
    if (error) *error = -ENOSPC;
    return NULL;
}

int vfs_readdir(struct vnode* dir, size_t index, struct dirent* entry) {
    if (!dir || !entry) return -EINVAL;
    if (dir->type != VNODE_DIR) return -ENOTDIR;
    const char* path = dir == &g_root ? "/" : dir->path;
    if (!path[0]) return -EINVAL;
    if (strcmp(path, "/tmp") == 0) return tmpfs_list(index, entry);

    char prefix[VFS_PREFIX_MAX];
    size_t prefix_length;
    if (!dir_prefix(path, prefix, &prefix_length)) return -ENAMETOOLONG;

    size_t seen = 0;
    size_t total = candidate_total();
    for (size_t i = 0; i < total; i++) {
        const char* candidate;
        int type;
        size_t length;
        bool nested;
        if (!candidate_at(i, &candidate, &type) ||
            !child_of(candidate, prefix, prefix_length, &length, &nested))
            continue;
        if (length >= sizeof(entry->d_name)) continue;

        /* Um subdiretorio aparece uma vez so, na primeira ocorrencia. */
        bool duplicate = false;
        for (size_t j = 0; j < i && !duplicate; j++) {
            const char* other;
            int other_type;
            size_t other_length;
            bool other_nested;
            if (!candidate_at(j, &other, &other_type) ||
                !child_of(other, prefix, prefix_length, &other_length,
                          &other_nested))
                continue;
            duplicate = other_length == length &&
                        strncmp(other + prefix_length,
                                candidate + prefix_length, length) == 0;
        }
        if (duplicate) continue;
        if (seen++ != index) continue;

        memset(entry, 0, sizeof(*entry));
        memcpy(entry->d_name, candidate + prefix_length, length);
        entry->d_type = (uint8_t)(nested ? VNODE_DIR : type);
        entry->d_ino = (uint32_t)(i + 1u);
        entry->d_off = (uint32_t)(index + 1u);
        entry->d_reclen = (uint16_t)sizeof(*entry);
        return 1;
    }
    return 0;
}

void vfs_init(void) {
    memset(&g_root, 0, sizeof(g_root));
    g_root.type = VNODE_DIR;
    g_root.name = "/";
    g_root.ops = &g_dir_ops;
    g_root.refcount = 1;
    memset(g_dirs, 0, sizeof(g_dirs));
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

struct vnode* vfs_open_err(const char* path, int flags, int* error) {
    if (error) *error = -ENOENT;
    if (!path) return NULL;
    struct vnode* vnode = NULL;
    if (strcmp(path, "/") == 0) vnode = &g_root;
    else if (strcmp(path, "/dev/console") == 0 ||
             strcmp(path, "/dev/tty") == 0) vnode = &g_console;
    else if (tmpfs_owns(path)) {
        vnode = tmpfs_lookup(path, flags, error);
        if (!vnode) return NULL;
    }
    else if ((vnode = procfs_lookup(path)) == NULL) {
        for (size_t i = 0; i < g_memfile_count; i++) {
            if (strcmp(path, g_memfiles[i].path) == 0) {
                vnode = &g_memfiles[i];
                break;
            }
        }
    }
    if (!vnode && is_directory(path)) {
        vnode = dir_vnode(path, error);
        if (!vnode) return NULL;
    }
    if (!vnode) return NULL;
    if (vnode->ops && vnode->ops->open) {
        int result = vnode->ops->open(vnode, flags);
        if (result < 0) {
            if (error) *error = result;
            return NULL;
        }
    }
    vnode->refcount++;
    if (error) *error = 0;
    return vnode;
}

struct vnode* vfs_open(const char* path, int flags) {
    return vfs_open_err(path, flags, NULL);
}

/* Remove um arquivo. Somente /tmp e gravavel; o resto e somente leitura. */
int vfs_unlink(const char* path) {
    if (!path) return -EINVAL;
    if (tmpfs_owns(path)) return tmpfs_unlink(path);
    return -EROFS;
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

/* ------------------------------------------------------------- release/stat */

void vfs_release(struct vnode* vnode) {
    if (!vnode) return;
    (void)vfs_close(vnode);
    if (vnode->ops && vnode->ops->release) vnode->ops->release(vnode);
}

int vfs_stat_vnode(struct vnode* vnode, struct tach_stat* info) {
    if (!vnode || !info) return -EINVAL;
    memset(info, 0, sizeof(*info));
    uint32_t permissions = 0644u;
    switch (vnode->type) {
        case VNODE_DIR:
            info->st_mode = TACH_S_IFDIR;
            permissions = 0755u;
            info->st_nlink = 2;
            break;
        case VNODE_CHAR:
            info->st_mode = TACH_S_IFCHR;
            permissions = 0666u;
            break;
        case VNODE_BLOCK:
            info->st_mode = TACH_S_IFBLK;
            permissions = 0660u;
            break;
        case VNODE_LINK:
            info->st_mode = TACH_S_IFLNK;
            permissions = 0777u;
            break;
        case VNODE_PIPE:
            info->st_mode = TACH_S_IFIFO;
            permissions = 0600u;
            break;
        default:
            info->st_mode = TACH_S_IFREG;
            if (strncmp(vnode->path, "/bin/", 5) == 0 ||
                strncmp(vnode->path, "/sbin/", 6) == 0)
                permissions = 0755u;      /* executaveis do initrd */
            else if (strncmp(vnode->path, "/proc/", 6) == 0)
                permissions = 0444u;      /* gerado pelo kernel, so leitura */
            break;
    }
    info->st_mode |= permissions;
    if (!info->st_nlink) info->st_nlink = 1;
    info->st_ino = (uint32_t)((uintptr_t)vnode >> 4);
    if (vnode->type != VNODE_DIR) info->st_size = (uint64_t)vnode->size;
    return 0;
}

int vfs_stat(const char* path, struct tach_stat* info) {
    int error = -ENOENT;
    struct vnode* vnode = vfs_open_err(path, O_RDONLY, &error);
    if (!vnode) return error < 0 ? error : -ENOENT;
    int result = vfs_stat_vnode(vnode, info);
    vfs_release(vnode);
    return result;
}

/* ----------------------------------------------------------------- caminhos */

int vfs_normalize_path(char* path) {
    if (!path || path[0] != '/') return -EINVAL;
    /* A escrita (indice `length`) nunca ultrapassa a leitura (`cursor`),
     * entao normalizar no mesmo buffer e seguro. */
    size_t length = 1;
    size_t cursor = 1;
    while (path[cursor]) {
        while (path[cursor] == '/') cursor++;
        if (!path[cursor]) break;
        size_t start = cursor;
        while (path[cursor] && path[cursor] != '/') cursor++;
        size_t part = cursor - start;

        if (part == 1 && path[start] == '.') continue;
        if (part == 2 && path[start] == '.' && path[start + 1] == '.') {
            while (length > 1 && path[length - 1] != '/') length--;
            if (length > 1) length--; /* remove a barra que antecede o componente */
            continue;
        }
        if (length > 1) path[length++] = '/';
        memmove(path + length, path + start, part);
        length += part;
    }
    path[length] = '\0';
    return 0;
}

int vfs_resolve_path(const char* cwd, const char* path, char* out,
                     size_t capacity) {
    if (!path || !out || !capacity) return -EINVAL;
    if (!path[0]) return -ENOENT;
    size_t used = 0;
    if (path[0] != '/') {
        if (!cwd || cwd[0] != '/') cwd = "/";
        size_t cwd_length = strlen(cwd);
        if (cwd_length + 1 >= capacity) return -ENAMETOOLONG;
        memcpy(out, cwd, cwd_length);
        used = cwd_length;
        if (used > 1) out[used++] = '/';
    }
    size_t length = strlen(path);
    if (used + length >= capacity) return -ENAMETOOLONG;
    memcpy(out + used, path, length + 1);
    return vfs_normalize_path(out);
}
