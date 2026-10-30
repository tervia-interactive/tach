/*
 * fs/tmpfs.h - Temporary filesystem (RAM)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_TMPFS_H
#define _FS_TMPFS_H

#include <fs/vfs.h>

void tmpfs_init(void);
struct vnode* tmpfs_create_root(void);

#endif /* _FS_TMPFS_H */
