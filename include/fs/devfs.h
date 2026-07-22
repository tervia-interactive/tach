/*
 * fs/devfs.h - Device filesystem (/dev)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_DEVFS_H
#define _FS_DEVFS_H

#include <fs/vfs.h>

void devfs_init(void);
struct vnode* devfs_create_device(const char* name, int type, void* data);
void devfs_register_device(const char* name, struct vnode_ops* ops);

#endif /* _FS_DEVFS_H */
