/*
 * fs/fat32.h - Driver FAT32 (somente leitura)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Camadas:
 *   1. Volume/arquivos/diretorios sobre um `struct blkdev` (fat32_mount,
 *      fat32_lookup, fat32_read, fat32_readdir). Nao depende do VFS.
 *   2. Adaptador para `struct vnode` (fat32_vnode_open/fat32_vnode_readdir).
 *      A integracao com a tabela de montagem do VFS e da etapa 4.
 *
 * Limites desta etapa: setor de 512 bytes, nomes comparados sem distincao
 * de maiusculas apenas em ASCII, nomes 8.3 com bytes >= 0x80 sao lidos como
 * Latin-1 (a codepage OEM nao e conhecida), entradas "." e ".." nunca sao
 * listadas e ".." tambem nao e aceito em caminhos (o VFS normaliza antes).
 * Toda a aritmetica e de 32 bits ou por deslocamento (sem divisao de 64
 * bits), entao linka em i686/arm32/riscv32 sem libgcc.
 */
#ifndef _FS_FAT32_H
#define _FS_FAT32_H

#include <kernel/types.h>
#include <drivers/block/blk.h>
#include <fs/vfs.h>

#define FAT32_NAME_MAX 255          /* bytes UTF-8, sem o NUL */

#define FAT32_ATTR_RO     0x01
#define FAT32_ATTR_HIDDEN 0x02
#define FAT32_ATTR_SYSTEM 0x04
#define FAT32_ATTR_DIR    0x10
#define FAT32_ATTR_ARCH   0x20

struct fat32_vol;                   /* opaco */

/* Metadados de uma entrada de diretorio. */
struct fat32_info {
    char name[FAT32_NAME_MAX + 1];  /* nome longo em UTF-8 (ou o 8.3) */
    char short_name[13];            /* "NOME.EXT" em ASCII, sempre presente */
    uint8_t attr;
    uint32_t first_cluster;         /* 0 = arquivo vazio */
    uint32_t size;                  /* 0 para diretorios */
    uint16_t wdate;                 /* ultima escrita, campos FAT crus */
    uint16_t wtime;
};

/* Cursor de leitura de diretorio (interno, exposto para fat32_node). */
struct fat32_dircur {
    uint32_t cluster;               /* 0 = fim do diretorio */
    uint32_t pos;                   /* entrada de 32 bytes dentro do cluster */
    uint32_t steps;                 /* clusters percorridos (anti-loop) */
};

/* Estado por arquivo/diretorio aberto: metadados + caches de percurso. */
struct fat32_node {
    struct fat32_info info;
    uint32_t chain_index;           /* cache: indice do cluster na cadeia */
    uint32_t chain_cluster;         /* cache: numero do cluster (0 = vazio) */
    size_t rd_next;                 /* readdir: indice que o cursor produz */
    struct fat32_dircur rd_cur;
    bool rd_valid;
};

/* ------------------------------------------------------------- volume */
/* Valida o boot sector e cria o volume. -EINVAL se nao for FAT32. */
int fat32_mount(struct blkdev* dev, struct fat32_vol** out);
/* -EBUSY enquanto houver vnodes abertos; senao libera o volume. */
int fat32_unmount(struct fat32_vol* vol);
const char* fat32_label(const struct fat32_vol* vol);
uint32_t fat32_cluster_count(const struct fat32_vol* vol);
uint32_t fat32_cluster_size(const struct fat32_vol* vol);

/* ------------------------------------------------- arquivos/diretorios */
/* `path` e relativo a raiz do volume ("a/b/c" ou "/a/b/c"); "" e "/" = raiz.
 * -ENOENT, -ENOTDIR, -ENAMETOOLONG, -EIO (FAT corrompida) ou erro de bloco. */
int fat32_lookup(struct fat32_vol* vol, const char* path,
                 struct fat32_info* out);
void fat32_node_init(struct fat32_node* node, const struct fat32_info* info);
/* Le ate `count` bytes a partir de `offset`. Devolve bytes lidos (0 no fim
 * do arquivo) ou -errno; leitura parcial devolve a contagem parcial. */
ssize_t fat32_read(struct fat32_vol* vol, struct fat32_node* node,
                   uint32_t offset, void* buf, size_t count);
/* Entrada numero `index` (0, 1, ...) de um diretorio. 1 = preenchida,
 * 0 = fim, -errno em erro. Leituras sequenciais custam O(1) por chamada. */
int fat32_readdir(struct fat32_vol* vol, struct fat32_node* node,
                  size_t index, struct fat32_info* out);

/* ---------------------------------------------------- adaptador vnode */
/* Abre `path` (relativo a raiz do volume). Somente leitura: escrita,
 * O_TRUNC, O_APPEND ou O_CREAT em arquivo inexistente -> -EROFS (-EISDIR para
 * escrita em diretorio). O vnode e alocado aqui e liberado pelo hook
 * `release` quando o ultimo descritor e descartado (open conta 1; `dup`
 * soma, `release` subtrai). */
struct vnode* fat32_vnode_open(struct fat32_vol* vol, const char* path,
                               int flags, int* error);
/* Equivalente a vfs_readdir para um vnode criado por fat32_vnode_open. */
int fat32_vnode_readdir(struct vnode* dir, size_t index, struct dirent* entry);

#endif /* _FS_FAT32_H */
