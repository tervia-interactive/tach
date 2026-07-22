/*
 * fs/procfs.h - Process introspection filesystem (/proc) — backs `ps`, `uname`
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_PROCFS_H
#define _FS_PROCFS_H

#include <fs/vfs.h>

void procfs_init(void);
struct vnode* procfs_create_process_entry(pid_t pid, const char* name);

#endif /* _FS_PROCFS_H */
