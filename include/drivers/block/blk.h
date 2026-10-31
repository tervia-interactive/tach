/*
 * drivers/block/blk.h - Block device layer, buffer cache and partitions
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Camada unica entre drivers de disco (ATA, AHCI, ramdisk) e sistemas de
 * arquivos. Todo acesso passa por um cache de setores write-back indexado
 * pelo disco FISICO (raiz) e LBA absoluto; por isso particoes e o disco
 * inteiro enxergam sempre os mesmos dados.
 *
 * Tamanho de setor logico fixo em 512 bytes (BLK_SECTOR_SIZE).
 */
#ifndef _DRIVERS_BLK_H
#define _DRIVERS_BLK_H

#include <kernel/types.h>

#define BLK_SECTOR_SIZE 512u
#define BLK_NAME_MAX    16
#define BLK_MAX_DEVICES 32

struct blkdev;

/* Operacoes do driver. `read`/`write` recebem LBA ABSOLUTO do proprio
 * dispositivo e ja validado contra `sectors`. `flush` e opcional. */
struct blkdev_ops {
    int (*read)(struct blkdev* dev, uint64_t lba, uint32_t count, void* buf);
    int (*write)(struct blkdev* dev, uint64_t lba, uint32_t count,
                 const void* buf);
    int (*flush)(struct blkdev* dev);
};

struct blkdev {
    char name[BLK_NAME_MAX];
    uint64_t sectors;            /* tamanho em setores de 512 bytes */
    bool readonly;
    const struct blkdev_ops* ops;/* NULL em particoes (usam a do pai) */
    void* priv;                  /* dado privado do driver */
    struct blkdev* parent;       /* disco inteiro, se for particao */
    uint64_t start;              /* LBA inicial dentro do pai */
    uint8_t part_type;           /* tipo MBR (0xEE-GPT mapeia p/ 0) */
    uint8_t part_guid[16];       /* GUID do tipo da particao GPT */
    bool is_gpt;
    bool owned;                  /* alocado pela camada de bloco (kfree) */
};

/* Buffer do cache. `data` e valido enquanto `refs` > 0 (bread/brelse). */
struct blkbuf {
    struct blkdev* dev;          /* disco raiz */
    uint64_t lba;                /* LBA absoluto no disco raiz */
    uint32_t refs;
    bool dirty;
    struct blkbuf* hnext;
    struct blkbuf* lru_prev;
    struct blkbuf* lru_next;
    uint8_t data[BLK_SECTOR_SIZE];
};

/* ------------------------------------------------------------ registro */
void blk_init(void);                       /* idempotente */
int blk_register(struct blkdev* dev);
int blk_unregister(struct blkdev* dev);    /* sincroniza, descarta cache */
struct blkdev* blk_find(const char* name);
struct blkdev* blk_get(size_t index);      /* NULL fora do intervalo */
size_t blk_count(void);

/* ------------------------------------------------------- I/O por setor */
int blk_read(struct blkdev* dev, uint64_t lba, uint32_t count, void* buf);
int blk_write(struct blkdev* dev, uint64_t lba, uint32_t count,
              const void* buf);
int blk_flush(struct blkdev* dev);         /* write-back + flush do driver */
int blk_sync_all(void);

/* -------------------------------------------------------- I/O por byte */
int blk_read_bytes(struct blkdev* dev, uint64_t offset, void* buf, size_t len);
int blk_write_bytes(struct blkdev* dev, uint64_t offset, const void* buf,
                    size_t len);

/* ------------------------------------------------- acesso a buffers */
/* bread: le o setor (relativo a `dev`) e o prende. Devolve 0 ou -errno. */
int bread(struct blkdev* dev, uint64_t lba, struct blkbuf** out);
/* bnew: como bread, mas sem ler do disco; o buffer vem ZERADO. Para setores
 * que serao totalmente sobrescritos (cluster novo, por exemplo). */
int bnew(struct blkdev* dev, uint64_t lba, struct blkbuf** out);
void bdirty(struct blkbuf* buf);
void brelse(struct blkbuf* buf);

/* ------------------------------------------------------ particoes */
/* Le MBR/GPT de `disk` e registra um blkdev por particao (hda -> hda1...).
 * Se o setor 0 for um boot sector FAT ("superfloppy") nao cria nada.
 * Devolve o numero de particoes criadas ou -errno. */
int blk_scan_partitions(struct blkdev* disk);
uint32_t blk_crc32(uint32_t crc, const void* data, size_t len);

/* --------------------------------------------------------- ramdisk */
/* Disco em memoria sobre uma regiao existente (ex.: modulo multiboot). */
struct blkdev* ramdisk_create(const char* name, void* mem, size_t size,
                              bool readonly);
/* Disco em memoria com armazenamento proprio (kzalloc), zerado. */
struct blkdev* ramdisk_alloc(const char* name, size_t size);
void ramdisk_destroy(struct blkdev* dev);

#endif /* _DRIVERS_BLK_H */
