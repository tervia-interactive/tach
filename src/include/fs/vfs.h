/*
 * fs/vfs.h - Virtual File System (inodes, dentries, file ops)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <kernel/types.h>

struct dirent {
    uint32_t d_ino;
    uint32_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[256];
};

#define VNODE_FILE  1
#define VNODE_DIR   2
#define VNODE_LINK  3
#define VNODE_CHAR  4
#define VNODE_BLOCK 5

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_ACCMODE 0x0003
#define O_CREAT  0x0200
#define O_TRUNC  0x0400
#define O_APPEND 0x0008

struct vnode {
    int type;
    char* name;
    struct vnode* parent;
    void* data;
    struct vnode_ops* ops;
    size_t refcount;
    size_t size;
    char path[128];
    char name_storage[64];
};

struct vnode_ops {
    int (*open)(struct vnode* vn, int flags);
    int (*close)(struct vnode* vn);
    ssize_t (*read)(struct vnode* vn, void* buf, size_t count, int64_t offset);
    ssize_t (*write)(struct vnode* vn, const void* buf, size_t count, int64_t offset);
    int (*readdir)(struct vnode* vn, struct dirent* entry, int64_t offset);
    struct vnode* (*lookup)(struct vnode* vn, const char* name);
    struct vnode* (*create)(struct vnode* vn, const char* name, int type);
    int (*unlink)(struct vnode* vn, const char* name);
    int (*truncate)(struct vnode* vn, size_t size);
    int (*sync)(struct vnode* vn);
};

void vfs_init(void);
struct vnode* vfs_root(void);
int vfs_mount(const char* path, struct vnode* root);
struct vnode* vfs_open(const char* path, int flags);
int vfs_close(struct vnode* vn);
ssize_t vfs_read(struct vnode* vn, void* buf, size_t count, int64_t offset);
ssize_t vfs_write(struct vnode* vn, const void* buf, size_t count, int64_t offset);
int vfs_readdir(struct vnode* vn, struct dirent* entry, int64_t offset);
int vfs_mkdir(const char* path);
int vfs_unlink(const char* path);
int vfs_sync(void);
struct vnode* vfs_console(void);
int vfs_register_memfile(const char* path, const void* data, size_t size);
int vfs_read_file(const char* path, const void** data, size_t* size);

#endif /* _FS_VFS_H */
