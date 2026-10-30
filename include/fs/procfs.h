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

/* Vnode de um arquivo /proc gerado sob demanda; NULL se o caminho nao existe. */
struct vnode* procfs_lookup(const char* path);

/* Enumeracao dos arquivos /proc (caminhos absolutos), usada para montar a
 * listagem dos diretorios /proc e /proc/self. */
size_t procfs_count(void);
const char* procfs_path(size_t index);

#endif /* _FS_PROCFS_H */
