#include <kernel/errno.h>
#include <kernel/string.h>
#include <proc/fd.h>

#define FD_TABLE_POOL_SIZE 32

static struct fd_table g_fd_tables[FD_TABLE_POOL_SIZE];
static bool g_fd_table_used[FD_TABLE_POOL_SIZE];

struct fd_table* fd_table_create(void) {
    for (size_t i = 0; i < FD_TABLE_POOL_SIZE; i++) {
        if (!g_fd_table_used[i]) {
            g_fd_table_used[i] = true;
            memset(&g_fd_tables[i], 0, sizeof(g_fd_tables[i]));
            g_fd_tables[i].refcount = 1;
            return &g_fd_tables[i];
        }
    }
    return NULL;
}

struct fd_table* fd_table_clone(const struct fd_table* source) {
    if (!source) return NULL;
    struct fd_table* clone = fd_table_create();
    if (!clone) return NULL;
    for (int fd = 0; fd < FD_TABLE_SIZE; fd++) {
        if (!source->entries[fd].vnode) continue;
        clone->entries[fd] = source->entries[fd];
        clone->entries[fd].vnode->refcount++;
    }
    return clone;
}

void fd_table_destroy(struct fd_table* table) {
    if (!table) {
        return;
    }
    for (size_t i = 0; i < FD_TABLE_POOL_SIZE; i++) {
        if (&g_fd_tables[i] == table) {
            for (int fd = 0; fd < FD_TABLE_SIZE; fd++) {
                fd_put(table, fd);
            }
            memset(table, 0, sizeof(*table));
            g_fd_table_used[i] = false;
            return;
        }
    }
}

int fd_alloc(struct fd_table* table, struct vnode* vnode, int flags) {
    if (!table || !vnode) {
        return -EINVAL;
    }
    for (int fd = 0; fd < FD_TABLE_SIZE; fd++) {
        if (!table->entries[fd].vnode) {
            table->entries[fd].vnode = vnode;
            table->entries[fd].flags = flags;
            table->entries[fd].offset = 0;
            vnode->refcount++;
            return fd;
        }
    }
    return -EMFILE;
}

struct fd_entry* fd_get(struct fd_table* table, int fd) {
    if (!table || fd < 0 || fd >= FD_TABLE_SIZE ||
        !table->entries[fd].vnode) {
        return NULL;
    }
    return &table->entries[fd];
}

void fd_put(struct fd_table* table, int fd) {
    struct fd_entry* entry = fd_get(table, fd);
    if (!entry) {
        return;
    }
    if (entry->vnode->refcount) {
        entry->vnode->refcount--;
    }
    memset(entry, 0, sizeof(*entry));
}

int fd_dup(struct fd_table* table, int oldfd, int newfd) {
    struct fd_entry* old = fd_get(table, oldfd);
    if (!old || newfd < 0 || newfd >= FD_TABLE_SIZE) {
        return -EBADF;
    }
    if (oldfd == newfd) {
        return newfd;
    }
    fd_put(table, newfd);
    table->entries[newfd] = *old;
    table->entries[newfd].vnode->refcount++;
    return newfd;
}
