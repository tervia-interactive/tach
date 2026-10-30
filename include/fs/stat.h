/*
 * fs/stat.h - Metadados de arquivo devolvidos por stat/fstat
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * O layout e fixo e identico em i686 e x86_64 (so campos de largura fixa,
 * sem padding implicito): 32 bytes, espelhado em user/include/tach.h.
 * Os bits de modo seguem os valores POSIX, mas com prefixo TACH_ para nao
 * colidir com <sys/stat.h> nos testes de host.
 */
#ifndef _FS_STAT_H
#define _FS_STAT_H

#include <kernel/types.h>

#define TACH_S_IFMT   0170000u
#define TACH_S_IFSOCK 0140000u
#define TACH_S_IFLNK  0120000u
#define TACH_S_IFREG  0100000u
#define TACH_S_IFBLK  0060000u
#define TACH_S_IFDIR  0040000u
#define TACH_S_IFCHR  0020000u
#define TACH_S_IFIFO  0010000u

#define TACH_S_ISREG(mode) (((mode) & TACH_S_IFMT) == TACH_S_IFREG)
#define TACH_S_ISDIR(mode) (((mode) & TACH_S_IFMT) == TACH_S_IFDIR)
#define TACH_S_ISCHR(mode) (((mode) & TACH_S_IFMT) == TACH_S_IFCHR)

struct tach_stat {
    uint32_t st_ino;      /* numero sintetico, estavel enquanto o vnode existir */
    uint32_t st_mode;     /* tipo (TACH_S_IF*) | permissoes */
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t st_reserved; /* sempre 0; mantem st_size alinhado em 8 bytes */
    uint64_t st_size;
};

#endif /* _FS_STAT_H */
