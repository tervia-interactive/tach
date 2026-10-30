/*
 * fs/tmpfs.c - /tmp: arquivos em RAM, com criacao, escrita e remocao
 *
 * Cada arquivo guarda seus bytes num bloco do heap do kernel (kmalloc), que
 * cresce sob demanda ate TMPFS_MAX_FILE_SIZE. A tabela de vnodes e pequena e
 * estatica de proposito: o .bss do kernel nao pode avancar sobre 0x400000,
 * onde o userspace e carregado (ver user/user.ld).
 *
 * "Aberto" e contado pelos hooks open/dup/release do vnode (release roda a
 * cada descritor descartado, inclusive no exit do processo), entao unlink so
 * libera o arquivo quando nenhum descritor aponta para ele.
 */
#include <kernel/types.h>
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/kheap.h>
#include <fs/tmpfs.h>

#define TMPFS_PREFIX     "/tmp/"
#define TMPFS_PREFIX_LEN 5u
#define TMPFS_MIN_CAPACITY 64u
#define TMPFS_LISTING_SIZE (TMPFS_MAX_FILES * (TMPFS_NAME_MAX + 1u) + 1u)

struct tmpfs_file {
    struct vnode vnode;
    uint8_t* data;
    size_t capacity;
    size_t opens;   /* descritores abertos apontando para este arquivo */
    bool used;
};

static struct tmpfs_file g_files[TMPFS_MAX_FILES];
static struct vnode g_dir;
static char g_listing[TMPFS_LISTING_SIZE];
static spinlock_t g_tmpfs_lock = SPINLOCK_INIT;

static struct tmpfs_file* file_of(struct vnode* vnode) {
    if (!vnode) return NULL;
    struct tmpfs_file* file = (struct tmpfs_file*)vnode;
    if (file < g_files || file >= g_files + TMPFS_MAX_FILES) return NULL;
    return file->used ? file : NULL;
}

/* ------------------------------------------------------------- arquivos */

static int file_open(struct vnode* vnode, int flags) {
    (void)flags;
    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = file_of(vnode);
    if (file) file->opens++;
    spinlock_unlock(&g_tmpfs_lock);
    return file ? 0 : -ENOENT;
}

static int file_close(struct vnode* vnode) {
    (void)vnode;
    return 0;
}

static void file_dup(struct vnode* vnode) {
    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = file_of(vnode);
    if (file) file->opens++;
    spinlock_unlock(&g_tmpfs_lock);
}

static void file_release(struct vnode* vnode) {
    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = file_of(vnode);
    if (file && file->opens) file->opens--;
    spinlock_unlock(&g_tmpfs_lock);
}

static ssize_t file_read(struct vnode* vnode, void* buffer, size_t count,
                         int64_t offset) {
    if (!buffer || offset < 0) return -EFAULT;
    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = file_of(vnode);
    if (!file) {
        spinlock_unlock(&g_tmpfs_lock);
        return -EBADF;
    }
    ssize_t result = 0;
    if ((uint64_t)offset < file->vnode.size) {
        size_t available = file->vnode.size - (size_t)offset;
        if (count > available) count = available;
        memcpy(buffer, file->data + (size_t)offset, count);
        result = (ssize_t)count;
    }
    spinlock_unlock(&g_tmpfs_lock);
    return result;
}

static ssize_t file_write(struct vnode* vnode, const void* buffer,
                          size_t count, int64_t offset) {
    if (!buffer || offset < 0) return -EFAULT;
    if (!count) return 0;
    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = file_of(vnode);
    if (!file) {
        spinlock_unlock(&g_tmpfs_lock);
        return -EBADF;
    }
    if ((uint64_t)offset >= TMPFS_MAX_FILE_SIZE) {
        spinlock_unlock(&g_tmpfs_lock);
        return -EFBIG;
    }
    size_t start = (size_t)offset;
    if (count > TMPFS_MAX_FILE_SIZE - start) count = TMPFS_MAX_FILE_SIZE - start;
    size_t end = start + count;

    if (end > file->capacity) {
        size_t wanted = file->capacity * 2u;
        if (wanted < end) wanted = end;
        if (wanted < TMPFS_MIN_CAPACITY) wanted = TMPFS_MIN_CAPACITY;
        if (wanted > TMPFS_MAX_FILE_SIZE) wanted = TMPFS_MAX_FILE_SIZE;
        uint8_t* grown = (uint8_t*)krealloc(file->data, wanted);
        if (!grown) {
            spinlock_unlock(&g_tmpfs_lock);
            return -ENOMEM;
        }
        file->data = grown;
        file->capacity = wanted;
    }
    /* Escrita alem do fim (apos lseek): o buraco le como zeros. */
    if (start > file->vnode.size)
        memset(file->data + file->vnode.size, 0, start - file->vnode.size);
    memcpy(file->data + start, buffer, count);
    if (end > file->vnode.size) file->vnode.size = end;
    spinlock_unlock(&g_tmpfs_lock);
    return (ssize_t)count;
}

static struct vnode_ops g_file_ops = {
    .open = file_open, .close = file_close,
    .read = file_read, .write = file_write, .readdir = NULL,
    .dup = file_dup, .release = file_release
};

/* ------------------------------------------------------------ diretorio */

static int dir_open(struct vnode* vnode, int flags) {
    (void)vnode;
    return (flags & O_ACCMODE) == O_RDONLY ? 0 : -EISDIR;
}

static int dir_close(struct vnode* vnode) {
    (void)vnode;
    return 0;
}

static ssize_t dir_read(struct vnode* vnode, void* buffer, size_t count,
                        int64_t offset) {
    (void)vnode;
    if (!buffer || offset < 0) return -EFAULT;
    spinlock_lock(&g_tmpfs_lock);
    size_t length = 0;
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
        if (!g_files[i].used) continue;
        const char* name = g_files[i].vnode.name;
        size_t name_length = strlen(name);
        memcpy(g_listing + length, name, name_length);
        length += name_length;
        g_listing[length++] = '\n';
    }
    ssize_t result = 0;
    if ((uint64_t)offset < length) {
        size_t available = length - (size_t)offset;
        if (count > available) count = available;
        memcpy(buffer, g_listing + (size_t)offset, count);
        result = (ssize_t)count;
    }
    spinlock_unlock(&g_tmpfs_lock);
    return result;
}

static struct vnode_ops g_dir_ops = {
    .open = dir_open, .close = dir_close,
    .read = dir_read, .write = NULL, .readdir = NULL
};

/* --------------------------------------------------------------- tabela */

void tmpfs_init(void) {
    spinlock_lock(&g_tmpfs_lock);
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
        if (g_files[i].used && g_files[i].data) kfree(g_files[i].data);
        memset(&g_files[i], 0, sizeof(g_files[i]));
    }
    memset(&g_dir, 0, sizeof(g_dir));
    strncpy(g_dir.path, "/tmp", sizeof(g_dir.path) - 1);
    g_dir.name = g_dir.path + 1;
    g_dir.type = VNODE_DIR;
    g_dir.parent = vfs_root();
    g_dir.ops = &g_dir_ops;
    g_dir.refcount = 1;
    spinlock_unlock(&g_tmpfs_lock);
}

struct vnode* tmpfs_create_root(void) { return &g_dir; }

int tmpfs_list(size_t index, struct dirent* entry) {
    if (!entry) return -EFAULT;
    int result = 0;
    size_t seen = 0;
    spinlock_lock(&g_tmpfs_lock);
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
        if (!g_files[i].used) continue;
        if (seen++ != index) continue;
        const char* name = g_files[i].vnode.name;
        size_t length = strlen(name); /* <= TMPFS_NAME_MAX, cabe em d_name */
        memset(entry, 0, sizeof(*entry));
        memcpy(entry->d_name, name, length);
        entry->d_type = VNODE_FILE;
        entry->d_ino = (uint32_t)(i + 1u);
        entry->d_off = (uint32_t)(index + 1u);
        entry->d_reclen = (uint16_t)sizeof(*entry);
        result = 1;
        break;
    }
    spinlock_unlock(&g_tmpfs_lock);
    return result;
}

int tmpfs_owns(const char* path) {
    if (!path) return 0;
    return strcmp(path, "/tmp") == 0 ||
           strncmp(path, TMPFS_PREFIX, TMPFS_PREFIX_LEN) == 0;
}

/* Valida o nome de arquivo sob /tmp/. 0 se ok, -errno se invalido. */
static int check_name(const char* name) {
    size_t length = strlen(name);
    if (!length) return -EISDIR;           /* "/tmp/" */
    if (length > TMPFS_NAME_MAX) return -ENAMETOOLONG;
    for (size_t i = 0; i < length; i++)
        if (name[i] == '/') return -ENOTDIR; /* /tmp e plano */
    return 0;
}

static struct tmpfs_file* find_file(const char* path) {
    for (size_t i = 0; i < TMPFS_MAX_FILES; i++)
        if (g_files[i].used && strcmp(g_files[i].vnode.path, path) == 0)
            return &g_files[i];
    return NULL;
}

struct vnode* tmpfs_lookup(const char* path, int flags, int* error) {
    int status = -ENOENT;
    struct vnode* result = NULL;
    if (!tmpfs_owns(path)) goto done;
    if (strcmp(path, "/tmp") == 0) {
        result = &g_dir;
        goto done;
    }
    status = check_name(path + TMPFS_PREFIX_LEN);
    if (status < 0) goto done;
    status = -ENOENT;

    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = find_file(path);
    if (file) {
        if ((flags & O_TRUNC) && (flags & O_ACCMODE) != O_RDONLY)
            file->vnode.size = 0;
        result = &file->vnode;
        status = 0;
    } else if (flags & O_CREAT) {
        status = -ENOSPC;
        for (size_t i = 0; i < TMPFS_MAX_FILES; i++) {
            if (g_files[i].used) continue;
            struct tmpfs_file* slot = &g_files[i];
            memset(slot, 0, sizeof(*slot));
            strncpy(slot->vnode.path, path, sizeof(slot->vnode.path) - 1);
            slot->vnode.name = slot->vnode.path + TMPFS_PREFIX_LEN;
            slot->vnode.type = VNODE_FILE;
            slot->vnode.parent = &g_dir;
            slot->vnode.ops = &g_file_ops;
            slot->vnode.refcount = 1;
            slot->used = true;
            result = &slot->vnode;
            status = 0;
            break;
        }
    }
    spinlock_unlock(&g_tmpfs_lock);
done:
    if (error) *error = result ? 0 : status;
    return result;
}

int tmpfs_unlink(const char* path) {
    if (!tmpfs_owns(path)) return -ENOENT;
    if (strcmp(path, "/tmp") == 0) return -EISDIR;
    int status = check_name(path + TMPFS_PREFIX_LEN);
    if (status < 0) return status;

    spinlock_lock(&g_tmpfs_lock);
    struct tmpfs_file* file = find_file(path);
    if (!file) {
        status = -ENOENT;
    } else if (file->opens) {
        status = -EBUSY;
    } else {
        uint8_t* data = file->data;
        memset(file, 0, sizeof(*file));
        spinlock_unlock(&g_tmpfs_lock);
        if (data) kfree(data);
        return 0;
    }
    spinlock_unlock(&g_tmpfs_lock);
    return status;
}
