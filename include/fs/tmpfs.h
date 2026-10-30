/*
 * fs/tmpfs.h - Temporary filesystem (RAM)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Sistema de arquivos em RAM, gravavel, montado em /tmp. E plano: /tmp e o
 * unico diretorio e os arquivos moram diretamente nele (/tmp/<nome>). Ler o
 * proprio /tmp devolve os nomes dos arquivos, um por linha.
 */
#ifndef _FS_TMPFS_H
#define _FS_TMPFS_H

#include <fs/vfs.h>

#define TMPFS_MAX_FILES     32
#define TMPFS_NAME_MAX      48
#define TMPFS_MAX_FILE_SIZE (1024u * 1024u)

void tmpfs_init(void);
struct vnode* tmpfs_create_root(void);

/* Verdadeiro se o caminho pertence ao tmpfs ("/tmp" ou "/tmp/..."). */
int tmpfs_owns(const char* path);

/* Abre (e, com O_CREAT, cria) um caminho do tmpfs. Em falha devolve NULL e
 * grava um -errno em *error (se nao for NULL). O refcount fica a cargo do
 * chamador (vfs_open). */
struct vnode* tmpfs_lookup(const char* path, int flags, int* error);

/* Remove um arquivo. 0 em sucesso, -errno em falha (-EBUSY se estiver
 * aberto por algum descritor). */
int tmpfs_unlink(const char* path);

#endif /* _FS_TMPFS_H */
