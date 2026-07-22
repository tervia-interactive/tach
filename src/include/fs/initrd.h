/*
 * fs/initrd.h - Initial ramdisk parser (cpio/tar)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_INITRD_H
#define _FS_INITRD_H

#include <fs/vfs.h>

void initrd_init(void* base, size_t size);
struct vnode* initrd_extract_to_tmpfs(void);

#endif /* _FS_INITRD_H */
