/*
 * fs/vfs.h - Virtual File System (inodes, dentries, file ops)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_VFS_H
#define _FS_VFS_H

#include <kernel/types.h>
#include <fs/stat.h>

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
#define VNODE_PIPE  6

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
};

struct vnode_ops {
    int (*open)(struct vnode* vn, int flags);
    int (*close)(struct vnode* vn);
    ssize_t (*read)(struct vnode* vn, void* buf, size_t count, int64_t offset);
    ssize_t (*write)(struct vnode* vn, const void* buf, size_t count, int64_t offset);
    int (*readdir)(struct vnode* vn, struct dirent* entry);
    /* Optional per-descriptor hooks, called by the fd layer (fd.c): `dup`
     * once for every extra descriptor slot referencing the vnode (dup, dup2,
     * fork) and `release` once for every slot that is dropped (close, exit).
     * Anonymous pipes use them to count open read/write ends. */
    void (*dup)(struct vnode* vn);
    void (*release)(struct vnode* vn);
};

void vfs_init(void);
struct vnode* vfs_root(void);
int vfs_mount(const char* path, struct vnode* root);
struct vnode* vfs_open(const char* path, int flags);
/* Como vfs_open, mas grava em *error o -errno da falha (ENOENT, ENOSPC...). */
struct vnode* vfs_open_err(const char* path, int flags, int* error);
int vfs_unlink(const char* path);

/* Diretorios: /, /dev, /proc, /proc/self e qualquer prefixo de arquivo do
 * initrd (/bin, /sbin...) existem de verdade; /tmp e tratado pelo tmpfs. */

/* Normaliza IN PLACE um caminho absoluto: colapsa "//", remove "." e a barra
 * final, resolve ".." (a raiz e seu proprio pai). O resultado nunca e maior
 * que a entrada. -EINVAL se `path` nao comecar com '/'. */
int vfs_normalize_path(char* path);

/* Monta em `out` o caminho absoluto e normalizado de `path`, relativo a `cwd`
 * (NULL ou invalido = "/"). `out` nao pode se sobrepor a `path`. -ENOENT para
 * caminho vazio, -ENAMETOOLONG se nao couber em `capacity`. */
int vfs_resolve_path(const char* cwd, const char* path, char* out,
                     size_t capacity);

/* Devolve em *entry a entrada de numero `index` (0, 1, 2...) do diretorio.
 * 1 = entrada preenchida, 0 = fim do diretorio, -errno em erro
 * (-ENOTDIR se `dir` nao for um diretorio). d_type recebe VNODE_*. */
int vfs_readdir(struct vnode* dir, size_t index, struct dirent* entry);

/* Metadados de um vnode ja aberto / de um caminho absoluto normalizado. */
int vfs_stat_vnode(struct vnode* vnode, struct tach_stat* info);
int vfs_stat(const char* path, struct tach_stat* info);

/* Desfaz um vfs_open_err que nao virou descritor (fecha e chama o hook
 * `release`, como close() faz para um descritor). */
void vfs_release(struct vnode* vnode);
int vfs_close(struct vnode* vn);
ssize_t vfs_read(struct vnode* vn, void* buf, size_t count, int64_t offset);
ssize_t vfs_write(struct vnode* vn, const void* buf, size_t count, int64_t offset);
struct vnode* vfs_console(void);
int vfs_register_memfile(const char* path, const void* data, size_t size);
int vfs_read_file(const char* path, const void** data, size_t* size);

#endif /* _FS_VFS_H */
