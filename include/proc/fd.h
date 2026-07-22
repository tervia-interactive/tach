/*
 * proc/fd.h - Per-process file descriptor table
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_FD_H
#define _PROC_FD_H

#include <kernel/types.h>
#include <fs/vfs.h>

#define FD_TABLE_SIZE 256
#define FD_FLAG_IN_USE 1

struct fd_entry {
    struct vnode* vnode;
    int flags;
    int64_t offset;
};

struct fd_table {
    struct fd_entry entries[FD_TABLE_SIZE];
    size_t refcount;
};

struct fd_table* fd_table_create(void);
void fd_table_destroy(struct fd_table* table);
int fd_alloc(struct fd_table* table, struct vnode* vnode, int flags);
struct fd_entry* fd_get(struct fd_table* table, int fd);
void fd_put(struct fd_table* table, int fd);
int fd_dup(struct fd_table* table, int oldfd, int newfd);

#endif /* _PROC_FD_H */
