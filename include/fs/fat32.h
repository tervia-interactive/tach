/*
 * fs/fat32.h - Driver FAT32 (leitura e escrita em arquivos existentes)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Camadas:
 *   1. Volume/arquivos/diretorios sobre um `struct blkdev` (fat32_mount,
 *      fat32_lookup, fat32_read, fat32_readdir). Nao depende do VFS.
 *   2. Adaptador para `struct vnode` (fat32_vnode_open/fat32_vnode_readdir).
 *      A integracao com a tabela de montagem do VFS e da etapa 4.
 *
 * Escrita (etapa 3.1): alocacao de clusters, fat32_write, fat32_truncate e
 * gravacao da entrada de diretorio do arquivo. Ainda NAO cria/remove/renomeia
 * entradas de diretorio (etapa 3.2). A FAT e gravada em todas as copias
 * (quando o espelhamento esta ativo) e o FSInfo e mantido em memoria e
 * gravado em fat32_sync()/fat32_unmount().
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
    uint32_t dir_cluster;           /* cluster do diretorio que guarda a entrada */
    uint32_t dir_pos;               /* indice (32 bytes) da entrada 8.3 nele;
                                     * dir_cluster == 0: raiz, sem entrada */
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
    bool dirty;                     /* size/first_cluster/data pendentes na
                                     * entrada de diretorio (fat32_node_sync) */
};

/* ------------------------------------------------------------- volume */
/* Valida o boot sector e cria o volume. -EINVAL se nao for FAT32. */
int fat32_mount(struct blkdev* dev, struct fat32_vol** out);
/* -EBUSY enquanto houver vnodes abertos; senao libera o volume. */
int fat32_unmount(struct fat32_vol* vol);
const char* fat32_label(const struct fat32_vol* vol);
uint32_t fat32_cluster_count(const struct fat32_vol* vol);
uint32_t fat32_cluster_size(const struct fat32_vol* vol);
bool fat32_readonly(const struct fat32_vol* vol);
/* Clusters livres. Usa o contador mantido em memoria (FSInfo); se ele for
 * desconhecido (volume somente leitura com FSInfo invalido) varre a FAT. */
int fat32_free_clusters(struct fat32_vol* vol, uint32_t* out);
/* Grava FSInfo (se mudou) e faz blk_flush do dispositivo. No-op em volume
 * somente leitura. fat32_unmount ja chama isto. */
int fat32_sync(struct fat32_vol* vol);
/* Relogio usado para wdate/wtime/adate. O padrao e a data fixa 2026-01-01
 * 00:00 (TODO: ligar a um RTC). `fn == NULL` volta ao padrao. */
void fat32_set_clock(void (*fn)(uint16_t* fat_date, uint16_t* fat_time));

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
/* Escreve `count` bytes em `offset`, estendendo o arquivo (e a cadeia de
 * clusters) quando preciso. Escrever alem do EOF preenche o intervalo com
 * zeros. Devolve bytes escritos (parcial se acabar o espaco/chegar a 4 GiB-1
 * depois de gravar algo) ou -errno: -EISDIR, -EROFS (volume RO), -EACCES
 * (atributo RO), -ENOSPC, -EFBIG. O tamanho e a data ficam no no em memoria;
 * a entrada de diretorio e gravada por fat32_node_sync(). */
ssize_t fat32_write(struct fat32_vol* vol, struct fat32_node* node,
                    uint32_t offset, const void* buf, size_t count);
/* Muda o tamanho: encolher libera a cauda da cadeia, crescer preenche com
 * zeros. 0 ou -errno (mesmos codigos de fat32_write). */
int fat32_truncate(struct fat32_vol* vol, struct fat32_node* node,
                   uint32_t new_size);
/* Grava no disco a entrada de diretorio (size, first_cluster, datas) se o
 * no estiver sujo. Nao faz flush do dispositivo (ver fat32_sync). */
int fat32_node_sync(struct fat32_vol* vol, struct fat32_node* node);

/* ------------------------------------------- operacoes de diretorio (3.2)
 * `path` relativo a raiz do volume. Serializadas por um flag no volume:
 * outra operacao de diretorio em curso => -EBUSY (nao ha sleep-lock).
 * Politica de arquivos abertos: unlink/rmdir/rename de uma entrada com vnode
 * aberto => -EBUSY (a entrada de diretorio guarda a posicao do no aberto).
 * Erros comuns: -EROFS, -ENOENT, -ENOTDIR, -EEXIST, -EINVAL (nome invalido),
 * -ENAMETOOLONG, -ENOSPC. Nomes: UTF-8, sem \\ / : * ? " < > | nem controles,
 * sem '.'/' ' no fim; 8.3 gerado com ~N em colisao e LFN quando preciso. */
/* Cria arquivo vazio (sem cluster). `out` (opcional) recebe a entrada. */
int fat32_create(struct fat32_vol* vol, const char* path, struct fat32_info* out);
int fat32_unlink(struct fat32_vol* vol, const char* path);   /* -EISDIR, -EACCES(RO) */
int fat32_mkdir(struct fat32_vol* vol, const char* path);    /* cria "." e ".." */
int fat32_rmdir(struct fat32_vol* vol, const char* path);    /* -ENOTEMPTY */
/* Renomeia/move arquivo ou diretorio. Destino existente: arquivo substitui
 * arquivo; diretorio so substitui diretorio vazio. -EINVAL ao mover um
 * diretorio para dentro de si mesmo. */
int fat32_rename(struct fat32_vol* vol, const char* old_path,
                 const char* new_path);

/* Entrada numero `index` (0, 1, ...) de um diretorio. 1 = preenchida,
 * 0 = fim, -errno em erro. Leituras sequenciais custam O(1) por chamada. */
int fat32_readdir(struct fat32_vol* vol, struct fat32_node* node,
                  size_t index, struct fat32_info* out);

/* ---------------------------------------------------- adaptador vnode */
/* Abre `path` (relativo a raiz do volume). Escrita/O_TRUNC exigem
 * volume gravavel (-EROFS), arquivo sem atributo RO (-EACCES) e nao-diretorio
 * (-EISDIR); O_TRUNC conta como escrita (e trunca para 0). O_CREAT em arquivo
 * inexistente: -EROFS em volume RO, cria via fat32_create. O vnode e alocado aqui e liberado pelo hook
 * `release` quando o ultimo descritor e descartado (open conta 1; `dup`
 * soma, `release` subtrai). */
struct vnode* fat32_vnode_open(struct fat32_vol* vol, const char* path,
                               int flags, int* error);
/* Equivalente a vfs_readdir para um vnode criado por fat32_vnode_open. */
int fat32_vnode_readdir(struct vnode* dir, size_t index, struct dirent* entry);

/* Monta `dev` (FAT32) em `path` na tabela de montagem do VFS (etapa 4.1). */
int fat32_vfs_mount(struct blkdev* dev, const char* path);

#endif /* _FS_FAT32_H */
