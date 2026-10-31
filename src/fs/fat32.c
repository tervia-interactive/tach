/*
 * fs/fat32.c - FAT32 (BPB, cadeia da FAT, diretorios, LFN, escrita em arquivos)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Todo acesso a disco passa pela camada de bloco (bread/blk_read). O estado
 * mutavel em memoria (vnodes abertos, contadores do FSInfo, caches de percurso
 * e metadados de cada no) e protegido por um spinlock curto; o spinlock nunca
 * e mantido durante E/S. A reserva de um cluster livre e atomica porque o
 * teste "entrada == 0" e a marca EOC acontecem sobre o buffer em cache da
 * FAT sob o spinlock (o cache e unico por setor).
 */
#include <kernel/types.h>
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <kernel/klog.h>
#include <mm/kheap.h>
#include <fs/fat32.h>

#define FAT32_MIN_CLUSTERS 65525u       /* abaixo disso e FAT12/FAT16 */
#define FAT32_MAX_CLUSTERS 0x0FFFFFF5u  /* clusters 2..0x0FFFFFF6 */
#define FAT32_EOC_MIN      0x0FFFFFF8u
#define FAT32_LFN_SLOTS    20u          /* 20 * 13 >= 255 unidades UTF-16 */
#define FAT32_EOC          0x0FFFFFFFu
#define FAT32_UNKNOWN      0xFFFFFFFFu  /* FSInfo: contador desconhecido */
#define FAT32_MAX_SIZE     0xFFFFFFFFu  /* limite de arquivo FAT: 4 GiB - 1 */

struct fat32_vnode;

struct fat32_vol {
    struct blkdev* dev;
    bool readonly;              /* dev->readonly */
    uint32_t num_fats;
    uint32_t fat_size;          /* setores por copia da FAT */
    uint32_t fat_active;        /* copia lida (0 com espelhamento) */
    bool mirror;                /* gravar em todas as copias */
    uint64_t fat0_lba;          /* inicio da 1a copia da FAT */
    uint64_t fat_lba;           /* inicio da FAT ativa */
    uint64_t fsinfo_lba;        /* 0 = sem FSInfo */
    uint32_t free_count;        /* FAT32_UNKNOWN se desconhecido (lock) */
    uint32_t next_free;         /* dica de alocacao (lock) */
    bool fsinfo_dirty;          /* (lock) */
    bool dir_busy;              /* operacao de diretorio em curso (lock) */
    uint64_t data_lba;          /* setor do cluster 2 */
    uint32_t clusters;          /* clusters de dados (numerados 2..clusters+1) */
    uint32_t root_cluster;
    uint32_t spc;               /* setores por cluster (potencia de 2) */
    uint32_t spc_shift;
    uint32_t entries_per_cluster;
    uint32_t open_nodes;        /* vnodes vivos (protegido por `lock`) */
    struct fat32_vnode* open_list;  /* vnodes vivos, p/ compartilhar (lock) */
    char label[12];
    spinlock_t lock;
};

/* --------------------------------------------------------------- helpers */

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}

static void put16(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t* p, uint32_t v) {
    put16(p, v);
    put16(p + 2, v >> 16);
}

static char fold(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static bool name_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (fold(*a) != fold(*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static uint64_t cluster_lba(const struct fat32_vol* vol, uint32_t cluster) {
    return vol->data_lba + ((uint64_t)(cluster - 2u) << vol->spc_shift);
}

/* ----------------------------------------------------------------- relogio */

static void (*g_clock)(uint16_t* date, uint16_t* time);

void fat32_set_clock(void (*fn)(uint16_t* fat_date, uint16_t* fat_time)) {
    g_clock = fn;
}

/* TODO: o kernel ainda nao tem RTC/epoch; ate la usa 2026-01-01 00:00:00
 * (data FAT = (2026-1980)<<9 | 1<<5 | 1). */
static void fat_now(uint16_t* date, uint16_t* time) {
    if (g_clock) {
        g_clock(date, time);
        return;
    }
    *date = (uint16_t)((46u << 9) | (1u << 5) | 1u);
    *time = 0;
}

/* ------------------------------------------------------------------ mount */

static int fat_count_free(struct fat32_vol* vol, uint32_t* out);

int fat32_mount(struct blkdev* dev, struct fat32_vol** out) {
    if (!dev || !out) return -EINVAL;
    uint8_t bs[BLK_SECTOR_SIZE];
    struct blkbuf* buf;
    int result = bread(dev, 0, &buf);
    if (result < 0) return result;
    memcpy(bs, buf->data, sizeof(bs));
    brelse(buf);

    if (bs[510] != 0x55 || bs[511] != 0xAA) return -EINVAL;
    if (le16(bs + 11) != BLK_SECTOR_SIZE) return -EINVAL;
    uint32_t spc = bs[13];
    if (spc == 0 || (spc & (spc - 1u)) != 0 || spc > 128u) return -EINVAL;
    uint32_t reserved = le16(bs + 14);
    uint32_t nfats = bs[16];
    if (reserved == 0 || nfats == 0 || nfats > 4u) return -EINVAL;
    if (le16(bs + 17) != 0 || le16(bs + 22) != 0) return -EINVAL; /* FAT12/16 */
    uint32_t total = le16(bs + 19) ? le16(bs + 19) : le32(bs + 32);
    uint32_t fat_size = le32(bs + 36);
    uint32_t ext_flags = le16(bs + 40);
    uint32_t root = le32(bs + 44);
    if (total == 0 || fat_size == 0) return -EINVAL;
    if (total > dev->sectors) return -EINVAL;

    uint64_t meta = (uint64_t)reserved + (uint64_t)nfats * fat_size;
    if (meta >= total) return -EINVAL;
    uint32_t shift = 0;
    while ((1u << shift) < spc) shift++;
    uint32_t clusters = (total - (uint32_t)meta) >> shift;
    if (clusters < FAT32_MIN_CLUSTERS) return -EINVAL;
    if (clusters > FAT32_MAX_CLUSTERS) clusters = FAT32_MAX_CLUSTERS;
    if ((uint64_t)fat_size * 128u < (uint64_t)clusters + 2u) return -EINVAL;
    if (root < 2u || root >= clusters + 2u) return -EINVAL;

    uint32_t active = 0;                 /* bit 7: so uma FAT esta ativa */
    if (ext_flags & 0x80u) {
        active = ext_flags & 0x0Fu;
        if (active >= nfats) return -EINVAL;
    }

    struct fat32_vol* vol = kzalloc(sizeof(*vol));
    if (!vol) return -ENOMEM;
    vol->dev = dev;
    vol->readonly = dev->readonly;
    vol->num_fats = nfats;
    vol->fat_size = fat_size;
    vol->fat_active = active;
    vol->mirror = (ext_flags & 0x80u) == 0;
    vol->fat0_lba = reserved;
    vol->fat_lba = (uint64_t)reserved + (uint64_t)active * fat_size;
    vol->free_count = FAT32_UNKNOWN;
    vol->next_free = 2u;
    vol->data_lba = meta;
    vol->clusters = clusters;
    vol->root_cluster = root;
    vol->spc = spc;
    vol->spc_shift = shift;
    vol->entries_per_cluster = spc * 16u;
    spinlock_init(&vol->lock);

    if (bs[66] == 0x29) {                /* rotulo valido so com esta marca */
        size_t length = 11;
        while (length && bs[71 + length - 1] == ' ') length--;
        for (size_t i = 0; i < length; i++) {
            uint8_t c = bs[71 + i];
            vol->label[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
    }

    /* FSInfo: so contadores/dicas; qualquer inconsistencia -> recalcula. */
    uint32_t fsinfo = le16(bs + 48);
    if (fsinfo >= 1u && fsinfo < reserved) {
        struct blkbuf* fb;
        if (bread(dev, fsinfo, &fb) == 0) {
            const uint8_t* f = fb->data;
            if (le32(f) == 0x41615252u && le32(f + 484) == 0x61417272u &&
                le32(f + 508) == 0xAA550000u) {
                vol->fsinfo_lba = fsinfo;
                uint32_t fc = le32(f + 488), nf = le32(f + 492);
                if (fc <= clusters) vol->free_count = fc;
                if (nf >= 2u && nf < clusters + 2u) vol->next_free = nf;
            }
            brelse(fb);
        }
    }
    if (vol->free_count == FAT32_UNKNOWN && !vol->readonly) {
        uint32_t free_clusters;
        int counted = fat_count_free(vol, &free_clusters);
        if (counted < 0) {
            kfree(vol);
            return counted;
        }
        vol->free_count = free_clusters;
        vol->fsinfo_dirty = vol->fsinfo_lba != 0;
    }

    klog_info("fat32", "%s: %u clusters de %u bytes, raiz no cluster %u%s",
              dev->name, (unsigned)clusters, (unsigned)(spc * BLK_SECTOR_SIZE),
              (unsigned)root, vol->readonly ? " (somente leitura)" : "");
    *out = vol;
    return 0;
}

int fat32_unmount(struct fat32_vol* vol) {
    if (!vol) return -EINVAL;
    spinlock_lock(&vol->lock);
    uint32_t open_nodes = vol->open_nodes;
    spinlock_unlock(&vol->lock);
    if (open_nodes) return -EBUSY;
    int result = fat32_sync(vol);
    if (result < 0) return result;      /* volume continua montado */
    kfree(vol);
    return 0;
}

const char* fat32_label(const struct fat32_vol* vol) {
    return vol ? vol->label : "";
}

uint32_t fat32_cluster_count(const struct fat32_vol* vol) {
    return vol ? vol->clusters : 0;
}

uint32_t fat32_cluster_size(const struct fat32_vol* vol) {
    return vol ? vol->spc * BLK_SECTOR_SIZE : 0;
}

bool fat32_readonly(const struct fat32_vol* vol) {
    return vol ? vol->readonly : true;
}

/* -------------------------------------------------------- cadeia da FAT */

/* 1 = *next recebe o proximo cluster; 0 = fim da cadeia; <0 = erro. Livre,
 * reservado, defeituoso ou fora do volume contam como FAT corrompida. */
static int fat_next(struct fat32_vol* vol, uint32_t cluster, uint32_t* next) {
    if (cluster < 2u || cluster >= vol->clusters + 2u) return -EIO;
    struct blkbuf* buf;
    int result = bread(vol->dev, vol->fat_lba + (cluster >> 7), &buf);
    if (result < 0) return result;
    uint32_t value = le32(buf->data + (cluster & 127u) * 4u) & 0x0FFFFFFFu;
    brelse(buf);
    if (value >= FAT32_EOC_MIN) return 0;
    if (value < 2u || value >= vol->clusters + 2u) return -EIO;
    *next = value;
    return 1;
}

/* --------------------------------------------------- escrita: FAT/clusters */

/* Conta entradas livres da FAT ativa (varredura; so no mount sem FSInfo). */
static int fat_count_free(struct fat32_vol* vol, uint32_t* out) {
    uint32_t end = vol->clusters + 2u;           /* exclusivo */
    uint32_t count = 0;
    for (uint32_t base = 0; base < end; base += 128u) {
        struct blkbuf* buf;
        int result = bread(vol->dev, vol->fat_lba + (base >> 7), &buf);
        if (result < 0) return result;
        for (uint32_t i = 0; i < 128u && base + i < end; i++) {
            if (base + i < 2u) continue;
            if ((le32(buf->data + i * 4u) & 0x0FFFFFFFu) == 0) count++;
        }
        brelse(buf);
    }
    *out = count;
    return 0;
}

int fat32_free_clusters(struct fat32_vol* vol, uint32_t* out) {
    if (!vol || !out) return -EINVAL;
    spinlock_lock(&vol->lock);
    uint32_t known = vol->free_count;
    spinlock_unlock(&vol->lock);
    if (known == FAT32_UNKNOWN) {
        int result = fat_count_free(vol, &known);
        if (result < 0) return result;
        spinlock_lock(&vol->lock);
        if (vol->free_count == FAT32_UNKNOWN) vol->free_count = known;
        spinlock_unlock(&vol->lock);
    }
    *out = known;
    return 0;
}

/* Grava `value` (28 bits) na copia `copy` da FAT, preservando os 4 bits
 * altos. Um buffer preso por vez. */
static int fat_put_copy(struct fat32_vol* vol, uint32_t copy, uint32_t cluster,
                        uint32_t value) {
    struct blkbuf* buf;
    int result = bread(vol->dev,
                       vol->fat0_lba + (uint64_t)copy * vol->fat_size +
                           (cluster >> 7),
                       &buf);
    if (result < 0) return result;
    uint8_t* e = buf->data + (cluster & 127u) * 4u;
    put32(e, (le32(e) & 0xF0000000u) | (value & 0x0FFFFFFFu));
    bdirty(buf);
    brelse(buf);
    return 0;
}

/* Grava a entrada em todas as copias (ou so na ativa sem espelhamento). */
static int fat_set(struct fat32_vol* vol, uint32_t cluster, uint32_t value) {
    if (cluster < 2u || cluster >= vol->clusters + 2u) return -EIO;
    if (!vol->mirror) return fat_put_copy(vol, vol->fat_active, cluster, value);
    for (uint32_t copy = 0; copy < vol->num_fats; copy++) {
        int result = fat_put_copy(vol, copy, cluster, value);
        if (result < 0) return result;
    }
    return 0;
}

/* Reserva um cluster livre e o marca como fim de cadeia (EOC) em todas as
 * copias. A reserva e atomica: o teste "entrada == 0" e a marca EOC rodam
 * sobre o buffer em cache da FAT ativa dentro do spinlock. Busca circular a
 * partir da dica do FSInfo. -ENOSPC so depois de percorrer o volume todo.
 * O conteudo do cluster NAO e alterado (ver cluster_zero). */
static int cluster_alloc(struct fat32_vol* vol, uint32_t* out) {
    if (vol->readonly) return -EROFS;
    uint32_t end = vol->clusters + 2u;           /* exclusivo */
    spinlock_lock(&vol->lock);
    uint32_t cl = vol->next_free;
    spinlock_unlock(&vol->lock);
    if (cl < 2u || cl >= end) cl = 2u;
    uint32_t remaining = vol->clusters;

    while (remaining) {
        uint32_t first = cl & 127u;
        uint32_t span = 128u - first;            /* entradas deste setor */
        if (span > remaining) span = remaining;
        if (span > end - cl) span = end - cl;
        struct blkbuf* buf;
        int result = bread(vol->dev, vol->fat_lba + (cl >> 7), &buf);
        if (result < 0) return result;

        uint32_t found = 0;
        spinlock_lock(&vol->lock);
        for (uint32_t i = 0; i < span; i++) {
            uint8_t* e = buf->data + (first + i) * 4u;
            uint32_t old = le32(e);
            if ((old & 0x0FFFFFFFu) != 0) continue;
            put32(e, (old & 0xF0000000u) | FAT32_EOC);
            found = cl + i;
            break;
        }
        if (found) {
            if (vol->free_count != FAT32_UNKNOWN && vol->free_count)
                vol->free_count--;
            vol->next_free = found + 1u < end ? found + 1u : 2u;
            vol->fsinfo_dirty = true;
        }
        spinlock_unlock(&vol->lock);

        if (found) {
            bdirty(buf);
            brelse(buf);
            if (vol->mirror) {
                for (uint32_t copy = 0; copy < vol->num_fats; copy++) {
                    if (copy == vol->fat_active) continue;
                    result = fat_put_copy(vol, copy, found, FAT32_EOC);
                    if (result < 0) {            /* desfaz a reserva */
                        (void)fat_put_copy(vol, vol->fat_active, found, 0);
                        spinlock_lock(&vol->lock);
                        if (vol->free_count != FAT32_UNKNOWN) vol->free_count++;
                        spinlock_unlock(&vol->lock);
                        return result;
                    }
                }
            }
            *out = found;
            return 0;
        }
        brelse(buf);
        cl += span;
        if (cl >= end) cl = 2u;
        remaining -= span;
    }
    return -ENOSPC;
}

/* Libera a cadeia que comeca em `first` (0 = nada). Para no primeiro erro de
 * gravacao e devolve -errno; os clusters ja liberados continuam contados. Um
 * elo invalido (livre/reservado/fora do volume) termina a cadeia com -EIO
 * depois de liberar o cluster atual. */
static int cluster_free_chain(struct fat32_vol* vol, uint32_t first) {
    uint32_t cl = first;
    uint32_t freed = 0, lowest = 0xFFFFFFFFu;
    int error = 0;
    while (cl >= 2u && cl < vol->clusters + 2u) {
        if (freed >= vol->clusters) {            /* cadeia em loop */
            error = -EIO;
            break;
        }
        uint32_t next = 0;
        int link = fat_next(vol, cl, &next);
        int result = fat_set(vol, cl, 0);
        if (result < 0) {
            error = result;
            break;
        }
        freed++;
        if (cl < lowest) lowest = cl;
        if (link <= 0) {
            if (link < 0) error = link;
            break;
        }
        cl = next;
    }
    if (freed) {
        spinlock_lock(&vol->lock);
        if (vol->free_count != FAT32_UNKNOWN) {
            vol->free_count += freed;
            if (vol->free_count > vol->clusters) vol->free_count = vol->clusters;
        }
        if (lowest < vol->next_free) vol->next_free = lowest;
        vol->fsinfo_dirty = true;
        spinlock_unlock(&vol->lock);
    }
    return error;
}

/* Zera todos os setores do cluster (sem ler do disco). */
static int cluster_zero(struct fat32_vol* vol, uint32_t cluster) {
    uint64_t lba = cluster_lba(vol, cluster);
    for (uint32_t i = 0; i < vol->spc; i++) {
        struct blkbuf* buf;
        int result = bnew(vol->dev, lba + i, &buf);   /* zerado e sujo */
        if (result < 0) return result;
        brelse(buf);
    }
    return 0;
}

/* ------------------------------------------------------------ diretorios */

static void cur_init(struct fat32_dircur* cur, uint32_t first_cluster) {
    cur->cluster = first_cluster;
    cur->pos = 0;
    cur->steps = 0;
}

/* 1 = entrada copiada para e[32]; 0 = fim da cadeia; <0 = erro. */
static int cur_peek(struct fat32_vol* vol, const struct fat32_dircur* cur,
                    uint8_t* e) {
    if (cur->cluster == 0) return 0;
    struct blkbuf* buf;
    int result = bread(vol->dev, cluster_lba(vol, cur->cluster) + (cur->pos >> 4),
                       &buf);
    if (result < 0) return result;
    memcpy(e, buf->data + (cur->pos & 15u) * 32u, 32);
    brelse(buf);
    return 1;
}

static int cur_advance(struct fat32_vol* vol, struct fat32_dircur* cur) {
    if (++cur->pos < vol->entries_per_cluster) return 0;
    cur->pos = 0;
    if (++cur->steps > vol->clusters) return -EIO;      /* cadeia em loop */
    uint32_t next = 0;
    int result = fat_next(vol, cur->cluster, &next);
    if (result < 0) return result;
    cur->cluster = result ? next : 0;
    return 0;
}

static uint8_t short_checksum(const uint8_t* name11) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1u) << 7) + (sum >> 1) + name11[i]);
    return sum;
}

/* UTF-16 (ate `max_units` unidades ou o primeiro 0) -> UTF-8. Devolve o
 * tamanho sem o NUL, ou -1 se nao couber em `cap` ou ficar vazio. */
static int utf16_to_utf8(const uint16_t* units, uint32_t max_units, char* out,
                         size_t cap) {
    size_t n = 0;
    for (uint32_t i = 0; i < max_units && units[i]; i++) {
        uint32_t cp = units[i];
        if (cp >= 0xD800u && cp < 0xDC00u) {
            if (i + 1 < max_units && units[i + 1] >= 0xDC00u &&
                units[i + 1] < 0xE000u) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (units[i + 1] - 0xDC00u);
                i++;
            } else {
                cp = '?';
            }
        } else if (cp >= 0xDC00u && cp < 0xE000u) {
            cp = '?';
        }
        size_t need = cp < 0x80u ? 1u : cp < 0x800u ? 2u : cp < 0x10000u ? 3u : 4u;
        if (n + need >= cap) return -1;
        if (need == 1) {
            out[n++] = (char)cp;
        } else if (need == 2) {
            out[n++] = (char)(0xC0u | (cp >> 6));
            out[n++] = (char)(0x80u | (cp & 0x3Fu));
        } else if (need == 3) {
            out[n++] = (char)(0xE0u | (cp >> 12));
            out[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
            out[n++] = (char)(0x80u | (cp & 0x3Fu));
        } else {
            out[n++] = (char)(0xF0u | (cp >> 18));
            out[n++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
            out[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
            out[n++] = (char)(0x80u | (cp & 0x3Fu));
        }
    }
    out[n] = '\0';
    return n ? (int)n : -1;
}

/* Preenche info->short_name (ASCII) e info->name (Latin-1 -> UTF-8) a partir
 * da entrada 8.3 `e`, respeitando os bits de caixa do Windows NT (e[12]). */
static void build_short_name(const uint8_t* e, struct fat32_info* info) {
    uint8_t raw[11];
    memcpy(raw, e, 11);
    if (raw[0] == 0x05) raw[0] = 0xE5;           /* 0xE5 real, escapado */
    size_t base = 8, ext = 3;
    while (base && raw[base - 1] == ' ') base--;
    while (ext && raw[8 + ext - 1] == ' ') ext--;
    bool lower_base = (e[12] & 0x08) != 0;
    bool lower_ext = (e[12] & 0x10) != 0;

    size_t s = 0, n = 0;
    for (size_t i = 0; i < base + ext; i++) {
        bool in_ext = i >= base;
        if (in_ext && i == base) {               /* ponto antes da extensao */
            info->short_name[s++] = '.';
            info->name[n++] = '.';
        }
        uint8_t c = raw[in_ext ? 8 + (i - base) : i];
        if (c >= 'A' && c <= 'Z' && (in_ext ? lower_ext : lower_base))
            c = (uint8_t)(c + ('a' - 'A'));
        if (c >= 0x80) {
            info->short_name[s++] = '_';
            info->name[n++] = (char)(0xC0u | (c >> 6));
            info->name[n++] = (char)(0x80u | (c & 0x3Fu));
        } else {
            info->short_name[s++] = (char)c;
            info->name[n++] = (char)c;
        }
    }
    info->short_name[s] = '\0';
    info->name[n] = '\0';
}

/* Proxima entrada visivel do diretorio (arquivo ou subdiretorio), com o nome
 * longo montado quando houver LFN valido. 1 = *info preenchido; 0 = fim;
 * <0 = erro. Em erro o cursor fica em estado indefinido. */
static int dir_next(struct fat32_vol* vol, struct fat32_dircur* cur,
                    struct fat32_info* info) {
    static const uint8_t lfn_offsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22,
                                            24, 28, 30};
    uint16_t units[FAT32_LFN_SLOTS * 13u];
    memset(units, 0, sizeof(units));
    uint32_t pending = 0;        /* ordinal do ultimo LFN aceito (0 = nenhum) */
    uint32_t total_slots = 0;
    uint8_t checksum = 0;
    uint8_t e[32];

    for (;;) {
        int result = cur_peek(vol, cur, e);
        if (result <= 0) return result;
        if (e[0] == 0x00) {                      /* fim do diretorio */
            cur->cluster = 0;
            return 0;
        }
        uint32_t ent_cluster = cur->cluster;     /* posicao desta entrada */
        uint32_t ent_pos = cur->pos;
        result = cur_advance(vol, cur);
        if (result < 0) return result;
        if (e[0] == 0xE5) {                      /* apagada */
            pending = 0;
            continue;
        }
        uint8_t attr = e[11];

        if ((attr & 0x3Fu) == 0x0Fu) {           /* entrada de nome longo */
            uint32_t seq = e[0];
            uint32_t slot;
            if (e[12] != 0 || e[26] != 0 || e[27] != 0) {
                pending = 0;
                continue;
            }
            if (seq & 0x40u) {                   /* primeira = a ultima do nome */
                slot = seq & 0x3Fu;
                if (slot == 0 || slot > FAT32_LFN_SLOTS) {
                    pending = 0;
                    continue;
                }
                total_slots = slot;
                checksum = e[13];
            } else {
                if (pending < 2u || seq != pending - 1u || e[13] != checksum) {
                    pending = 0;
                    continue;
                }
                slot = seq;
            }
            pending = slot;
            for (uint32_t i = 0; i < 13u; i++)
                units[(slot - 1u) * 13u + i] = le16(e + lfn_offsets[i]);
            continue;
        }

        if (attr & 0x08u) {                      /* rotulo de volume */
            pending = 0;
            continue;
        }
        if (e[0] == '.' && (e[1] == ' ' || (e[1] == '.' && e[2] == ' '))) {
            pending = 0;                         /* "." e ".." */
            continue;
        }

        build_short_name(e, info);
        if (pending == 1u && short_checksum(e) == checksum) {
            char longname[sizeof(info->name)];
            if (utf16_to_utf8(units, total_slots * 13u, longname,
                              sizeof(longname)) > 0)
                strcpy(info->name, longname);
        }
        info->attr = attr;
        info->first_cluster = (uint32_t)le16(e + 20) << 16 | le16(e + 26);
        info->size = (attr & FAT32_ATTR_DIR) ? 0 : le32(e + 28);
        info->wtime = le16(e + 22);
        info->wdate = le16(e + 24);
        info->dir_cluster = ent_cluster;
        info->dir_pos = ent_pos;
        return 1;
    }
}

static int dir_find(struct fat32_vol* vol, uint32_t dir_cluster,
                    const char* name, struct fat32_info* out) {
    struct fat32_dircur cur;
    cur_init(&cur, dir_cluster);
    for (;;) {
        int result = dir_next(vol, &cur, out);
        if (result < 0) return result;
        if (result == 0) return -ENOENT;
        if (name_eq(out->name, name) || name_eq(out->short_name, name))
            return 0;
    }
}

/* ---------------------------------------------------------------- lookup */

int fat32_lookup(struct fat32_vol* vol, const char* path,
                 struct fat32_info* out) {
    if (!vol || !path || !out) return -EINVAL;
    struct fat32_info current;
    memset(&current, 0, sizeof(current));
    current.attr = FAT32_ATTR_DIR;
    current.first_cluster = vol->root_cluster;

    const char* p = path;
    for (;;) {
        while (*p == '/') p++;
        if (!*p) break;
        const char* end = p;
        while (*end && *end != '/') end++;
        size_t length = (size_t)(end - p);
        if (length > FAT32_NAME_MAX) return -ENAMETOOLONG;
        if (length == 1 && p[0] == '.') {
            p = end;
            continue;
        }
        if (!(current.attr & FAT32_ATTR_DIR)) return -ENOTDIR;
        char component[FAT32_NAME_MAX + 1];
        memcpy(component, p, length);
        component[length] = '\0';
        struct fat32_info next;
        int result = dir_find(vol, current.first_cluster, component, &next);
        if (result < 0) return result;
        current = next;
        p = end;
    }
    *out = current;
    return 0;
}

void fat32_node_init(struct fat32_node* node, const struct fat32_info* info) {
    memset(node, 0, sizeof(*node));
    node->info = *info;
}

/* ------------------------------------------------------------------ read */

ssize_t fat32_read(struct fat32_vol* vol, struct fat32_node* node,
                   uint32_t offset, void* buffer, size_t count) {
    if (!vol || !node || (!buffer && count)) return -EINVAL;
    if (node->info.attr & FAT32_ATTR_DIR) return -EISDIR;
    uint32_t size = node->info.size;
    if (offset >= size || count == 0) return 0;
    if (count > (size_t)(size - offset)) count = (size_t)(size - offset);
    if (count > 0x7FFFFFFFu) count = 0x7FFFFFFFu;

    uint32_t cluster_shift = vol->spc_shift + 9u;
    uint32_t cluster_mask = (1u << cluster_shift) - 1u;
    uint32_t index = offset >> cluster_shift;
    uint32_t at, cluster;

    spinlock_lock(&vol->lock);
    if (node->chain_cluster && node->chain_index <= index) {
        at = node->chain_index;
        cluster = node->chain_cluster;
    } else {
        at = 0;
        cluster = node->info.first_cluster;
    }
    spinlock_unlock(&vol->lock);

    if (cluster < 2u) return -EIO;               /* tamanho > 0 sem cadeia */
    while (at < index) {                         /* limitado por `index` */
        uint32_t next = 0;
        int result = fat_next(vol, cluster, &next);
        if (result < 0) return result;
        if (result == 0) return -EIO;            /* cadeia menor que o tamanho */
        cluster = next;
        at++;
    }

    uint8_t* out = buffer;
    size_t done = 0;
    uint32_t pos = offset;
    int error = 0;
    while (done < count) {
        uint32_t in_cluster = pos & cluster_mask;
        uint32_t sector = in_cluster >> 9;
        uint32_t in_sector = in_cluster & (BLK_SECTOR_SIZE - 1u);
        uint64_t lba = cluster_lba(vol, cluster) + sector;
        size_t chunk;
        if (in_sector == 0 && count - done >= BLK_SECTOR_SIZE) {
            /* Sequencia de setores inteiros dentro do cluster: uma chamada. */
            uint32_t want = (uint32_t)((count - done) >> 9);
            if (want > vol->spc - sector) want = vol->spc - sector;
            error = blk_read(vol->dev, lba, want, out + done);
            chunk = (size_t)want << 9;
        } else {
            struct blkbuf* buf;
            error = bread(vol->dev, lba, &buf);
            if (error < 0) break;
            chunk = BLK_SECTOR_SIZE - in_sector;
            if (chunk > count - done) chunk = count - done;
            memcpy(out + done, buf->data + in_sector, chunk);
            brelse(buf);
        }
        if (error < 0) break;
        done += chunk;
        pos += (uint32_t)chunk;
        if (done < count && (pos & cluster_mask) == 0) {  /* proximo cluster */
            uint32_t next = 0;
            int result = fat_next(vol, cluster, &next);
            if (result <= 0) {
                error = result < 0 ? result : -EIO;
                break;
            }
            cluster = next;
            at++;
        }
    }

    spinlock_lock(&vol->lock);
    node->chain_index = at;
    node->chain_cluster = cluster;
    spinlock_unlock(&vol->lock);
    if (done) return (ssize_t)done;
    return error < 0 ? error : -EIO;
}

/* --------------------------------------------------------------- readdir */

int fat32_readdir(struct fat32_vol* vol, struct fat32_node* node, size_t index,
                  struct fat32_info* out) {
    if (!vol || !node || !out) return -EINVAL;
    if (!(node->info.attr & FAT32_ATTR_DIR)) return -ENOTDIR;

    struct fat32_dircur cur;
    size_t next_index;
    spinlock_lock(&vol->lock);
    if (node->rd_valid && node->rd_next <= index) {
        cur = node->rd_cur;
        next_index = node->rd_next;
    } else {
        cur_init(&cur, node->info.first_cluster);
        next_index = 0;
    }
    spinlock_unlock(&vol->lock);

    for (;;) {
        int result = dir_next(vol, &cur, out);
        if (result < 0) return result;
        bool found = result > 0 && next_index == index;
        if (result > 0) next_index++;
        if (result == 0 || found) {
            spinlock_lock(&vol->lock);
            node->rd_cur = cur;
            node->rd_next = next_index;
            node->rd_valid = true;
            spinlock_unlock(&vol->lock);
            return found ? 1 : 0;
        }
    }
}

/* ----------------------------------------------------------------- escrita */

struct dent_meta {                      /* copia dos campos que vao ao disco */
    uint32_t dir_cluster, dir_pos, first_cluster, size;
    uint16_t wdate, wtime;
};

/* Reescreve first_cluster/size/datas na entrada 8.3 (e liga o bit ARQUIVO).
 * -ENOENT se a entrada nao existir mais (apagada/LFN). */
static int dirent_store(struct fat32_vol* vol, const struct dent_meta* m) {
    if (m->dir_cluster == 0) return 0;           /* raiz: nao tem entrada */
    if (m->dir_cluster < 2u || m->dir_cluster >= vol->clusters + 2u ||
        m->dir_pos >= vol->entries_per_cluster)
        return -EIO;
    struct blkbuf* buf;
    int result = bread(vol->dev,
                       cluster_lba(vol, m->dir_cluster) + (m->dir_pos >> 4),
                       &buf);
    if (result < 0) return result;
    uint8_t* e = buf->data + (m->dir_pos & 15u) * 32u;
    if (e[0] == 0x00 || e[0] == 0xE5 || (e[11] & 0x3Fu) == 0x0Fu) {
        brelse(buf);
        return -ENOENT;
    }
    put16(e + 20, m->first_cluster >> 16);
    put16(e + 26, m->first_cluster);
    put32(e + 28, m->size);
    put16(e + 22, m->wtime);
    put16(e + 24, m->wdate);
    put16(e + 18, m->wdate);                     /* ultimo acesso */
    e[11] = (uint8_t)(e[11] | FAT32_ATTR_ARCH);
    bdirty(buf);
    brelse(buf);
    return 0;
}

int fat32_node_sync(struct fat32_vol* vol, struct fat32_node* node) {
    if (!vol || !node) return -EINVAL;
    struct dent_meta meta;
    spinlock_lock(&vol->lock);
    if (!node->dirty) {
        spinlock_unlock(&vol->lock);
        return 0;
    }
    meta.dir_cluster = node->info.dir_cluster;
    meta.dir_pos = node->info.dir_pos;
    meta.first_cluster = node->info.first_cluster;
    meta.size = node->info.size;
    meta.wdate = node->info.wdate;
    meta.wtime = node->info.wtime;
    node->dirty = false;
    spinlock_unlock(&vol->lock);

    int result = dirent_store(vol, &meta);
    if (result < 0) {
        spinlock_lock(&vol->lock);
        node->dirty = true;
        spinlock_unlock(&vol->lock);
    }
    return result;
}

/* Marca o no como modificado agora (data/hora + atributo arquivo). `lock`
 * NAO deve estar preso. `new_size` (se != NULL) so cresce. */
static void node_touch(struct fat32_vol* vol, struct fat32_node* node,
                       const uint32_t* grow_to) {
    uint16_t date, time;
    fat_now(&date, &time);
    spinlock_lock(&vol->lock);
    if (grow_to && *grow_to > node->info.size) node->info.size = *grow_to;
    node->info.wdate = date;
    node->info.wtime = time;
    node->info.attr = (uint8_t)(node->info.attr | FAT32_ATTR_ARCH);
    node->dirty = true;
    spinlock_unlock(&vol->lock);
}

/* Aloca um cluster novo, zera se `zero` e o liga depois de `prev` (0 = nao
 * liga: primeiro cluster do arquivo). Em erro nada fica alocado. */
static int chain_grow(struct fat32_vol* vol, uint32_t prev, bool zero,
                      uint32_t* out) {
    uint32_t fresh;
    int result = cluster_alloc(vol, &fresh);
    if (result < 0) return result;
    if (zero) result = cluster_zero(vol, fresh);
    if (result == 0 && prev) result = fat_set(vol, prev, fresh);
    if (result < 0) {
        (void)cluster_free_chain(vol, fresh);
        return result;
    }
    *out = fresh;
    return 0;
}

/* Motor de escrita: grava `count` bytes de `src` (NULL = zeros) a partir de
 * `offset`, alocando clusters conforme preciso. Nao mexe em info.size (o
 * chamador atualiza com o que foi escrito). `*done_out` = bytes gravados,
 * mesmo em erro. Clusters totalmente cobertos pela escrita nao sao zerados
 * antes; os demais sim (nunca vazam dados antigos do disco). */
static int file_put(struct fat32_vol* vol, struct fat32_node* node,
                    uint32_t offset, const uint8_t* src, uint32_t count,
                    uint32_t* done_out) {
    uint32_t shift = vol->spc_shift + 9u;
    uint32_t cmask = (1u << shift) - 1u;
    uint32_t csize = cmask + 1u;
    uint32_t index = offset >> shift;
    bool first_covered = (offset & cmask) == 0 && count >= csize;
    uint32_t done = 0, at, cluster;
    int error = 0;
    *done_out = 0;

    spinlock_lock(&vol->lock);
    if (node->chain_cluster && node->chain_index <= index) {
        at = node->chain_index;
        cluster = node->chain_cluster;
    } else {
        at = 0;
        cluster = node->info.first_cluster;
    }
    spinlock_unlock(&vol->lock);

    if (cluster < 2u) {                          /* arquivo ainda sem cadeia */
        error = chain_grow(vol, 0, !(index == 0 && first_covered), &cluster);
        if (error) return error;
        at = 0;
        spinlock_lock(&vol->lock);
        node->info.first_cluster = cluster;
        node->chain_index = 0;
        node->chain_cluster = cluster;
        node->dirty = true;
        spinlock_unlock(&vol->lock);
    }
    while (at < index) {                         /* ate o cluster do offset */
        uint32_t next = 0;
        int link = fat_next(vol, cluster, &next);
        if (link < 0) return link;
        if (link == 0) {                         /* cadeia acabou: estende */
            error = chain_grow(vol, cluster, !(at + 1u == index && first_covered),
                               &next);
            if (error) return error;
        }
        cluster = next;
        at++;
    }

    while (done < count) {
        uint32_t pos = offset + done;
        uint32_t in_cluster = pos & cmask;
        uint32_t sector = in_cluster >> 9;
        uint32_t in_sector = in_cluster & (BLK_SECTOR_SIZE - 1u);
        uint64_t lba = cluster_lba(vol, cluster) + sector;
        uint32_t remaining = count - done;
        uint32_t chunk;
        if (in_sector == 0 && remaining >= BLK_SECTOR_SIZE) {
            uint32_t want = remaining >> 9;      /* setores inteiros */
            if (want > vol->spc - sector) want = vol->spc - sector;
            chunk = want << 9;
            if (src) {
                error = blk_write(vol->dev, lba, want, src + done);
            } else {
                for (uint32_t w = 0; w < want && !error; w++) {
                    struct blkbuf* zb;
                    error = bnew(vol->dev, lba + w, &zb);   /* zerado, sujo */
                    if (!error) brelse(zb);
                }
            }
        } else {
            struct blkbuf* buf;
            chunk = BLK_SECTOR_SIZE - in_sector;
            if (chunk > remaining) chunk = remaining;
            error = bread(vol->dev, lba, &buf);
            if (!error) {
                if (src) memcpy(buf->data + in_sector, src + done, chunk);
                else memset(buf->data + in_sector, 0, chunk);
                bdirty(buf);
                brelse(buf);
            }
        }
        if (error) break;
        done += chunk;
        if (done < count && ((offset + done) & cmask) == 0) {  /* proximo */
            uint32_t next = 0;
            int link = fat_next(vol, cluster, &next);
            if (link < 0) {
                error = link;
                break;
            }
            if (link == 0) {
                error = chain_grow(vol, cluster, (count - done) < csize, &next);
                if (error) break;
            }
            cluster = next;
            at++;
        }
    }

    spinlock_lock(&vol->lock);                   /* cache do ultimo cluster */
    node->chain_index = at;
    node->chain_cluster = cluster;
    spinlock_unlock(&vol->lock);
    *done_out = done;
    return error;
}

/* Verificacoes comuns de escrita no no. */
static int write_check(const struct fat32_vol* vol, const struct fat32_node* node) {
    if (!vol || !node) return -EINVAL;
    if (node->info.attr & FAT32_ATTR_DIR) return -EISDIR;
    if (vol->readonly) return -EROFS;
    if (node->info.attr & FAT32_ATTR_RO) return -EACCES;
    return 0;
}

ssize_t fat32_write(struct fat32_vol* vol, struct fat32_node* node,
                    uint32_t offset, const void* buffer, size_t count) {
    int check = write_check(vol, node);
    if (check < 0) return check;
    if (!buffer && count) return -EINVAL;
    if (count == 0) return 0;
    if (offset == FAT32_MAX_SIZE) return -EFBIG;
    if (count > 0x7FFFFFFFu) count = 0x7FFFFFFFu;
    if (count > (size_t)(FAT32_MAX_SIZE - offset))
        count = (size_t)(FAT32_MAX_SIZE - offset);   /* escrita parcial */

    uint32_t size = node->info.size;
    if (offset > size) {                         /* buraco: preenche de zeros */
        uint32_t hole_done;
        int result = file_put(vol, node, size, NULL, offset - size, &hole_done);
        if (hole_done) {
            uint32_t grown = size + hole_done;
            node_touch(vol, node, &grown);
        }
        if (result < 0) return result;
    }

    uint32_t done;
    int result = file_put(vol, node, offset, buffer, (uint32_t)count, &done);
    uint32_t end = offset + done;
    node_touch(vol, node, done ? &end : NULL);
    if (done) return (ssize_t)done;
    return result < 0 ? result : -EIO;
}

int fat32_truncate(struct fat32_vol* vol, struct fat32_node* node,
                   uint32_t new_size) {
    int check = write_check(vol, node);
    if (check < 0) return check;
    uint32_t size = node->info.size;
    if (new_size == size) return 0;

    if (new_size > size) {                       /* crescer = zeros */
        uint32_t done;
        int result = file_put(vol, node, size, NULL, new_size - size, &done);
        if (done) {
            uint32_t grown = size + done;
            node_touch(vol, node, &grown);
        }
        return result;
    }

    uint32_t shift = vol->spc_shift + 9u;
    uint32_t cmask = (1u << shift) - 1u;
    uint32_t keep = (new_size >> shift) + ((new_size & cmask) ? 1u : 0u);

    /* 1. localiza o ultimo cluster que fica e o inicio da cauda a liberar. */
    uint32_t old_first = node->info.first_cluster;
    uint32_t last = 0, tail = 0;
    if (keep && old_first >= 2u) {
        uint32_t at, cluster;
        spinlock_lock(&vol->lock);
        if (node->chain_cluster && node->chain_index <= keep - 1u) {
            at = node->chain_index;
            cluster = node->chain_cluster;
        } else {
            at = 0;
            cluster = old_first;
        }
        spinlock_unlock(&vol->lock);
        bool short_chain = false;
        while (at < keep - 1u) {
            uint32_t next = 0;
            int link = fat_next(vol, cluster, &next);
            if (link < 0) return link;
            if (link == 0) {                     /* cadeia menor que o tamanho */
                short_chain = true;
                break;
            }
            cluster = next;
            at++;
        }
        if (!short_chain) {
            uint32_t next = 0;
            int link = fat_next(vol, cluster, &next);
            if (link < 0) return link;
            last = cluster;
            if (link > 0) tail = next;
        }
    } else if (!keep) {
        tail = old_first;
    }

    /* 2. grava a entrada de diretorio ANTES de mexer na FAT (se algo falhar
     * no meio, sobra cluster perdido, nunca cadeia compartilhada). */
    uint16_t date, time;
    fat_now(&date, &time);
    uint32_t saved_size, saved_first;
    uint16_t saved_date, saved_time;
    bool saved_dirty;
    spinlock_lock(&vol->lock);
    saved_size = node->info.size;
    saved_first = node->info.first_cluster;
    saved_date = node->info.wdate;
    saved_time = node->info.wtime;
    saved_dirty = node->dirty;
    node->info.size = new_size;
    if (!keep) node->info.first_cluster = 0;
    node->info.wdate = date;
    node->info.wtime = time;
    node->info.attr = (uint8_t)(node->info.attr | FAT32_ATTR_ARCH);
    node->dirty = true;
    if (node->chain_index >= keep) {             /* cache apontaria p/ a cauda */
        node->chain_index = 0;
        node->chain_cluster = 0;
    }
    spinlock_unlock(&vol->lock);
    int result = fat32_node_sync(vol, node);
    if (result < 0) {                            /* desfaz o estado em memoria */
        spinlock_lock(&vol->lock);
        node->info.size = saved_size;
        node->info.first_cluster = saved_first;
        node->info.wdate = saved_date;
        node->info.wtime = saved_time;
        node->dirty = saved_dirty;
        node->chain_index = 0;
        node->chain_cluster = 0;
        spinlock_unlock(&vol->lock);
        return result;
    }

    /* 3. novo fim de cadeia e libera a cauda. */
    if (keep && last) {
        result = fat_set(vol, last, FAT32_EOC);
        if (result < 0) return result;
    }
    if (tail) result = cluster_free_chain(vol, tail);
    return result;
}

int fat32_sync(struct fat32_vol* vol) {
    if (!vol) return -EINVAL;
    if (vol->readonly) return 0;
    uint32_t free_count, next_free;
    bool dirty;
    spinlock_lock(&vol->lock);
    dirty = vol->fsinfo_dirty;
    free_count = vol->free_count;
    next_free = vol->next_free;
    vol->fsinfo_dirty = false;
    spinlock_unlock(&vol->lock);

    if (dirty && vol->fsinfo_lba) {
        struct blkbuf* buf;
        int result = bread(vol->dev, vol->fsinfo_lba, &buf);
        if (result < 0) {
            spinlock_lock(&vol->lock);
            vol->fsinfo_dirty = true;
            spinlock_unlock(&vol->lock);
            return result;
        }
        put32(buf->data + 488, free_count);
        put32(buf->data + 492, next_free);
        bdirty(buf);
        brelse(buf);
    }
    return blk_flush(vol->dev);
}

/* ----------------------------------------------------------------- vnode */

#define FAT32_VNODE_PATH_MAX sizeof(((struct vnode*)0)->path)

struct fat32_vnode {
    struct vnode vn;            /* primeiro membro: cast direto */
    struct fat32_node node;
    struct fat32_vol* vol;
    size_t opens;               /* descritores apontando para este vnode */
    struct fat32_vnode* next;   /* lista vol->open_list */
};

static struct vnode_ops g_fat32_ops;

static struct fat32_vnode* fv_of(struct vnode* vn) {
    if (!vn || vn->ops != &g_fat32_ops) return NULL;
    return (struct fat32_vnode*)vn;
}

static ssize_t fv_read(struct vnode* vn, void* buffer, size_t count,
                       int64_t offset) {
    struct fat32_vnode* fv = fv_of(vn);
    if (!fv) return -EINVAL;
    if (offset < 0) return -EINVAL;
    if (offset > (int64_t)0xFFFFFFFFu) return 0;
    return fat32_read(fv->vol, &fv->node, (uint32_t)offset, buffer, count);
}

static ssize_t fv_write(struct vnode* vn, const void* buffer, size_t count,
                        int64_t offset) {
    struct fat32_vnode* fv = fv_of(vn);
    if (!fv) return -EINVAL;
    if (offset < 0) return -EINVAL;
    if (offset > (int64_t)FAT32_MAX_SIZE) return -EFBIG;
    ssize_t result = fat32_write(fv->vol, &fv->node, (uint32_t)offset, buffer,
                                 count);
    fv->vn.size = fv->node.info.size;
    return result;
}

static void fv_dup(struct vnode* vn) {
    struct fat32_vnode* fv = fv_of(vn);
    if (!fv) return;
    spinlock_lock(&fv->vol->lock);
    fv->opens++;
    spinlock_unlock(&fv->vol->lock);
}

/* Ultimo descritor: grava a entrada de diretorio e libera o vnode. O no
 * continua na lista (e contado em open_nodes) ate o fim, entao o volume nao
 * pode ser desmontado no meio do flush. Se alguem reabriu o arquivo nesse
 * intervalo (opens > 0) o vnode fica vivo para ele. */
static void fv_release(struct vnode* vn) {
    struct fat32_vnode* fv = fv_of(vn);
    if (!fv) return;
    struct fat32_vol* vol = fv->vol;
    bool last = false;
    spinlock_lock(&vol->lock);
    if (fv->opens && --fv->opens == 0) last = true;
    spinlock_unlock(&vol->lock);
    if (!last) return;

    int result = fat32_node_sync(vol, &fv->node);
    if (result < 0)
        klog_warn("fat32", "%s: falha ao gravar entrada de diretorio (%d)",
                  vol->dev->name, result);

    bool do_free = false;
    spinlock_lock(&vol->lock);
    if (fv->opens == 0) {
        struct fat32_vnode** link = &vol->open_list;
        while (*link && *link != fv) link = &(*link)->next;
        if (*link) *link = fv->next;
        vol->open_nodes--;
        do_free = true;
    }
    spinlock_unlock(&vol->lock);
    if (do_free) kfree(fv);
}

static struct vnode_ops g_fat32_ops = {
    .open = NULL, .close = NULL, .read = fv_read, .write = fv_write,
    .readdir = NULL, .dup = fv_dup, .release = fv_release,
    .readdir_at = fat32_vnode_readdir,
};

/* Dois descritores abertos para o mesmo arquivo compartilham UM vnode/no
 * (mesmo size, cadeia e entrada de diretorio); a chave e a posicao da
 * entrada 8.3 (a raiz tem dir_cluster 0). Chamar com `lock` preso. */
static struct fat32_vnode* open_find(struct fat32_vol* vol,
                                     const struct fat32_info* info) {
    for (struct fat32_vnode* fv = vol->open_list; fv; fv = fv->next) {
        const struct fat32_info* other = &fv->node.info;
        if (other->dir_cluster == info->dir_cluster &&
            (info->dir_cluster != 0 ? other->dir_pos == info->dir_pos
                                    : (other->attr & FAT32_ATTR_DIR) != 0))
            return fv;
    }
    return NULL;
}

struct vnode* fat32_vnode_open(struct fat32_vol* vol, const char* path,
                               int flags, int* error) {
    int ignored;
    if (!error) error = &ignored;
    if (!vol || !path) {
        *error = -EINVAL;
        return NULL;
    }
    if (strlen(path) >= FAT32_VNODE_PATH_MAX) {
        *error = -ENAMETOOLONG;
        return NULL;
    }
    struct fat32_info info;
    int result = fat32_lookup(vol, path, &info);
    if (result == -ENOENT && (flags & O_CREAT))
        result = fat32_create(vol, path, &info);     /* -EROFS em volume RO */
    if (result < 0) {
        *error = result;
        return NULL;
    }
    bool want_write = (flags & O_ACCMODE) != O_RDONLY || (flags & O_TRUNC);
    if (want_write) {
        if (info.attr & FAT32_ATTR_DIR) {
            *error = -EISDIR;
            return NULL;
        }
        if (vol->readonly) {
            *error = -EROFS;
            return NULL;
        }
        if (info.attr & FAT32_ATTR_RO) {
            *error = -EACCES;
            return NULL;
        }
    }

    struct fat32_vnode* fv = kzalloc(sizeof(*fv));
    if (!fv) {
        *error = -ENOMEM;
        return NULL;
    }
    strcpy(fv->vn.path, path);
    const char* base = fv->vn.path;
    for (const char* c = fv->vn.path; *c; c++)
        if (*c == '/' && c[1]) base = c + 1;
    fv->vn.name = (char*)base;
    fv->vn.type = (info.attr & FAT32_ATTR_DIR) ? VNODE_DIR : VNODE_FILE;
    fv->vn.ops = &g_fat32_ops;
    fv->vn.data = &fv->node;
    fv->vn.size = info.size;
    fat32_node_init(&fv->node, &info);
    fv->vol = vol;
    fv->opens = 1;

    spinlock_lock(&vol->lock);
    struct fat32_vnode* shared = open_find(vol, &info);
    if (shared) {
        shared->opens++;
    } else {
        fv->next = vol->open_list;
        vol->open_list = fv;
        vol->open_nodes++;
    }
    spinlock_unlock(&vol->lock);
    if (shared) {
        kfree(fv);
        fv = shared;
    }

    if ((flags & O_TRUNC) && fv->vn.type == VNODE_FILE) {
        result = fat32_truncate(vol, &fv->node, 0);
        fv->vn.size = fv->node.info.size;
        if (result < 0) {
            fv_release(&fv->vn);
            *error = result;
            return NULL;
        }
    }
    *error = 0;
    return &fv->vn;
}

int fat32_vnode_readdir(struct vnode* dir, size_t index, struct dirent* entry) {
    if (!entry) return -EINVAL;
    struct fat32_vnode* fv = fv_of(dir);
    if (!fv) return -EINVAL;
    if (fv->vn.type != VNODE_DIR) return -ENOTDIR;
    struct fat32_info info;
    int result = fat32_readdir(fv->vol, &fv->node, index, &info);
    if (result <= 0) return result;
    memset(entry, 0, sizeof(*entry));
    strcpy(entry->d_name, info.name);        /* name <= 255 bytes + NUL */
    entry->d_type = (info.attr & FAT32_ATTR_DIR) ? VNODE_DIR : VNODE_FILE;
    entry->d_ino = info.first_cluster;
    entry->d_off = (uint32_t)(index + 1u);
    entry->d_reclen = (uint16_t)sizeof(*entry);
    return 1;
}

/* ============================================ operacoes de diretorio (3.2) */

#define FAT32_DIR_MAX_ENTRIES 65536u    /* limite da especificacao FAT */
#define FAT32_PATH_BUF        256u      /* copia local do caminho do pai */

static int dirop_enter(struct fat32_vol* vol) {
    spinlock_lock(&vol->lock);
    if (vol->dir_busy) {
        spinlock_unlock(&vol->lock);
        return -EBUSY;
    }
    vol->dir_busy = true;
    spinlock_unlock(&vol->lock);
    return 0;
}

static void dirop_leave(struct fat32_vol* vol) {
    spinlock_lock(&vol->lock);
    vol->dir_busy = false;
    spinlock_unlock(&vol->lock);
}

static bool entry_is_open(struct fat32_vol* vol, const struct fat32_info* info) {
    spinlock_lock(&vol->lock);
    bool open = open_find(vol, info) != NULL;
    spinlock_unlock(&vol->lock);
    return open;
}

static bool str_same(const char* a, const char* b) {
    size_t n = strlen(a);
    return n == strlen(b) && memcmp(a, b, n) == 0;
}

/* ---- nomes ---- */

/* UTF-8 -> UTF-16. n unidades (>= 1) ou -errno. Rejeita sequencias invalidas,
 * controles e \ / : * ? " < > | */
static int utf8_to_utf16(const char* s, uint16_t* out, uint32_t cap) {
    const uint8_t* p = (const uint8_t*)s;
    uint32_t n = 0;
    while (*p) {
        uint32_t cp, extra;
        uint8_t c = *p++;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; extra = 3; }
        else return -EINVAL;
        for (uint32_t i = 0; i < extra; i++) {
            if ((*p & 0xC0) != 0x80) return -EINVAL;
            cp = (cp << 6) | (*p++ & 0x3Fu);
        }
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) ||
            (extra == 3 && cp < 0x10000) || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF))
            return -EINVAL;
        if (cp < 0x20 || cp == '\\' || cp == '/' || cp == ':' || cp == '*' ||
            cp == '?' || cp == '"' || cp == '<' || cp == '>' || cp == '|')
            return -EINVAL;
        if (cp >= 0x10000) {
            if (n + 2 > cap) return -ENAMETOOLONG;
            cp -= 0x10000;
            out[n++] = (uint16_t)(0xD800u | (cp >> 10));
            out[n++] = (uint16_t)(0xDC00u | (cp & 0x3FFu));
        } else {
            if (n + 1 > cap) return -ENAMETOOLONG;
            out[n++] = (uint16_t)cp;
        }
    }
    return (int)n;
}

/* Valida um nome de componente e devolve o numero de unidades UTF-16. */
static int name_units(const char* name, uint16_t* units) {
    size_t len = strlen(name);
    if (len == 0) return -EINVAL;
    if (len > FAT32_NAME_MAX) return -ENAMETOOLONG;
    if ((len == 1 && name[0] == '.') || (len == 2 && name[0] == '.' && name[1] == '.'))
        return -EINVAL;
    if (name[len - 1] == '.' || name[len - 1] == ' ') return -EINVAL;
    return utf8_to_utf16(name, units, 255u);
}

struct short_gen {
    uint8_t raw[11];
    uint8_t nt;             /* bits 0x08 (base minuscula) / 0x10 (ext) */
    bool lossy;             /* precisa de ~N */
    bool lfn;               /* precisa de entradas LFN */
    uint32_t base_len;      /* caracteres da base antes do ~N */
};

static bool short_char_ok(uint8_t c) {
    static const char extra[] = "!#$%&'()-@^_`{}~";
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
    for (size_t i = 0; extra[i]; i++)
        if ((uint8_t)extra[i] == c) return true;
    return false;
}

/* Base do nome 8.3 (maiusculas, invalidos -> '_', truncado); sem o ~N. */
static void short_gen_base(const char* name, struct short_gen* g) {
    size_t len = strlen(name);
    memset(g, 0, sizeof(*g));
    memset(g->raw, ' ', 11);
    size_t start = 0;
    while (start < len && (name[start] == '.' || name[start] == ' ')) {
        start++;
        g->lossy = true;
    }
    size_t dot = len;                          /* ultimo '.' apos `start` */
    for (size_t i = start; i < len; i++)
        if (name[i] == '.') dot = i;
    bool bl = false, bu = false, el = false, eu = false;
    uint32_t bi = 0, ei = 0;
    for (size_t i = start; i < dot; i++) {
        uint8_t c = (uint8_t)name[i];
        if (c == ' ' || c == '.') { g->lossy = true; continue; }
        if (c >= 'a' && c <= 'z') { bl = true; c = (uint8_t)(c - 32); }
        else if (c >= 'A' && c <= 'Z') bu = true;
        if (!short_char_ok(c)) { c = '_'; g->lossy = true; }
        if (bi < 8) g->raw[bi++] = c; else g->lossy = true;
    }
    for (size_t i = dot + 1; i < len; i++) {
        uint8_t c = (uint8_t)name[i];
        if (c == ' ') { g->lossy = true; continue; }
        if (c >= 'a' && c <= 'z') { el = true; c = (uint8_t)(c - 32); }
        else if (c >= 'A' && c <= 'Z') eu = true;
        if (!short_char_ok(c)) { c = '_'; g->lossy = true; }
        if (ei < 3) g->raw[8 + ei++] = c; else g->lossy = true;
    }
    if (bi == 0) { g->raw[bi++] = '_'; g->lossy = true; }
    g->base_len = bi;
    if ((bl && bu) || (el && eu)) g->lfn = true;   /* caixa mista: LFN */
    else g->nt = (uint8_t)((bl ? 0x08u : 0) | (el ? 0x10u : 0));
    if (g->lossy) g->lfn = true;
}

static int dir_has_short(struct fat32_vol* vol, uint32_t first,
                         const uint8_t* raw, bool* found) {
    struct fat32_dircur cur;
    uint8_t e[32];
    cur_init(&cur, first);
    *found = false;
    for (;;) {
        int result = cur_peek(vol, &cur, e);
        if (result <= 0) return result;
        if (e[0] == 0x00) return 0;
        if (e[0] != 0xE5 && (e[11] & 0x3Fu) != 0x0Fu && memcmp(e, raw, 11) == 0) {
            *found = true;
            return 0;
        }
        result = cur_advance(vol, &cur);
        if (result < 0) return result;
    }
}

/* Escolhe NOME~N.EXT livre quando o nome nao cabe em 8.3. */
static int short_pick_tail(struct fat32_vol* vol, uint32_t first,
                           struct short_gen* g) {
    uint8_t base[8], ext[3];
    memcpy(base, g->raw, 8);
    memcpy(ext, g->raw + 8, 3);
    for (uint32_t n = 1; n <= 999999u; n++) {
        char digits[8];
        uint32_t d = 0, v = n;
        while (v) { digits[d++] = (char)('0' + (v % 10u)); v /= 10u; }
        uint32_t keep = g->base_len;
        if (keep > 8u - 1u - d) keep = 8u - 1u - d;
        uint8_t raw[11];
        memset(raw, ' ', 11);
        memcpy(raw, base, keep);
        raw[keep] = '~';
        for (uint32_t i = 0; i < d; i++) raw[keep + 1u + i] = (uint8_t)digits[d - 1u - i];
        memcpy(raw + 8, ext, 3);
        bool found;
        int result = dir_has_short(vol, first, raw, &found);
        if (result < 0) return result;
        if (!found) {
            memcpy(g->raw, raw, 11);
            return 0;
        }
    }
    return -EEXIST;
}

/* ---- entradas de diretorio ---- */

static int ent_write(struct fat32_vol* vol, const struct fat32_dircur* c,
                     const uint8_t* e) {
    struct blkbuf* buf;
    int result = bread(vol->dev, cluster_lba(vol, c->cluster) + (c->pos >> 4), &buf);
    if (result < 0) return result;
    memcpy(buf->data + (c->pos & 15u) * 32u, e, 32);
    bdirty(buf);
    brelse(buf);
    return 0;
}

static int ent_mark_deleted(struct fat32_vol* vol, const struct fat32_dircur* c) {
    struct blkbuf* buf;
    int result = bread(vol->dev, cluster_lba(vol, c->cluster) + (c->pos >> 4), &buf);
    if (result < 0) return result;
    buf->data[(c->pos & 15u) * 32u] = 0xE5;
    bdirty(buf);
    brelse(buf);
    return 0;
}

/* Acha `need` entradas livres consecutivas (podem cruzar clusters); estende o
 * diretorio com clusters zerados quando falta espaco. Tudo apos a primeira
 * entrada 0x00 e tratado como livre (imagens validas tem zeros ali). */
static int dir_alloc_slots(struct fat32_vol* vol, uint32_t first, uint32_t need,
                           struct fat32_dircur* start_out) {
    struct fat32_dircur cur, run_start;
    uint32_t run = 0, last_cluster = first;
    bool tail = false;
    uint8_t e[32];
    memset(&run_start, 0, sizeof(run_start));
    cur_init(&cur, first);
    for (;;) {
        int result = cur_peek(vol, &cur, e);
        if (result < 0) return result;
        if (result == 0) break;                      /* fim da cadeia */
        last_cluster = cur.cluster;
        if (e[0] == 0x00) tail = true;
        if (tail || e[0] == 0xE5) {
            if (run == 0) run_start = cur;
            if (++run == need) {
                *start_out = run_start;
                return 0;
            }
        } else {
            run = 0;
        }
        result = cur_advance(vol, &cur);
        if (result < 0) return result;
    }
    uint32_t nclusters = cur.steps;
    while (run < need) {                             /* estende o diretorio */
        if ((nclusters + 1u) > FAT32_DIR_MAX_ENTRIES / vol->entries_per_cluster)
            return -ENOSPC;
        uint32_t fresh;
        int result = chain_grow(vol, last_cluster, true, &fresh);
        if (result < 0) return result;
        if (run == 0) {
            run_start.cluster = fresh;
            run_start.pos = 0;
            run_start.steps = 0;
        }
        run += vol->entries_per_cluster;
        last_cluster = fresh;
        nclusters++;
    }
    *start_out = run_start;
    return 0;
}

/* Cria a(s) entrada(s) (LFN + 8.3) de `name` no diretorio `dir_first`.
 * Nao verifica se o nome ja existe (o chamador faz). */
static int dir_add_entry(struct fat32_vol* vol, uint32_t dir_first,
                         const char* name, uint8_t attr, uint32_t first_cluster,
                         uint32_t size, uint16_t wdate, uint16_t wtime,
                         struct fat32_info* out) {
    static const uint8_t lfn_offsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22,
                                            24, 28, 30};
    uint16_t units[255];
    int nunits = name_units(name, units);
    if (nunits < 0) return nunits;

    struct short_gen g;
    short_gen_base(name, &g);
    if (g.lossy) {
        int result = short_pick_tail(vol, dir_first, &g);
        if (result < 0) return result;
    }
    uint32_t lfn_count = g.lfn ? ((uint32_t)nunits + 12u) / 13u : 0;
    uint8_t checksum = short_checksum(g.raw);

    uint8_t ents[FAT32_LFN_SLOTS + 1u][32];
    memset(ents, 0, sizeof(ents));
    for (uint32_t i = 0; i < lfn_count; i++) {
        uint32_t seq = lfn_count - i;                /* na ordem do disco */
        uint8_t* e = ents[i];
        e[0] = (uint8_t)(seq | (i == 0 ? 0x40u : 0u));
        e[11] = 0x0F;
        e[13] = checksum;
        for (uint32_t j = 0; j < 13u; j++) {
            uint32_t idx = (seq - 1u) * 13u + j;
            uint16_t u = idx < (uint32_t)nunits ? units[idx]
                         : (idx == (uint32_t)nunits ? 0x0000u : 0xFFFFu);
            put16(e + lfn_offsets[j], u);
        }
    }
    uint16_t now_date, now_time;
    fat_now(&now_date, &now_time);
    uint8_t* e = ents[lfn_count];
    memcpy(e, g.raw, 11);
    e[11] = attr;
    e[12] = g.nt;
    put16(e + 14, now_time);                         /* criacao */
    put16(e + 16, now_date);
    put16(e + 18, now_date);                         /* ultimo acesso */
    put16(e + 20, first_cluster >> 16);
    put16(e + 22, wtime);
    put16(e + 24, wdate);
    put16(e + 26, first_cluster);
    put32(e + 28, size);

    struct fat32_dircur cur;
    int result = dir_alloc_slots(vol, dir_first, lfn_count + 1u, &cur);
    if (result < 0) return result;
    uint32_t ent_cluster = 0, ent_pos = 0;
    for (uint32_t i = 0; i <= lfn_count; i++) {      /* 8.3 por ultimo */
        result = ent_write(vol, &cur, ents[i]);
        if (result < 0) return result;
        ent_cluster = cur.cluster;
        ent_pos = cur.pos;
        if (i < lfn_count) {
            result = cur_advance(vol, &cur);
            if (result < 0) return result;
        }
    }
    if (out) {
        memset(out, 0, sizeof(*out));
        build_short_name(e, out);
        if (g.lfn) strcpy(out->name, name);
        out->attr = attr;
        out->first_cluster = first_cluster;
        out->size = (attr & FAT32_ATTR_DIR) ? 0 : size;
        out->wdate = wdate;
        out->wtime = wtime;
        out->dir_cluster = ent_cluster;
        out->dir_pos = ent_pos;
    }
    return 0;
}

/* Apaga (0xE5) a entrada 8.3 de `info` e as LFN que a precedem (mesmo contiguo,
 * inclusive atraves de clusters). Nao libera a cadeia de dados. */
static int entry_delete(struct fat32_vol* vol, uint32_t parent_first,
                        const struct fat32_info* info) {
    struct fat32_dircur cur, start;
    bool have = false;
    uint8_t e[32];
    memset(&start, 0, sizeof(start));
    cur_init(&cur, parent_first);
    for (;;) {
        int result = cur_peek(vol, &cur, e);
        if (result < 0) return result;
        if (result == 0) return -ENOENT;
        if (cur.cluster == info->dir_cluster && cur.pos == info->dir_pos) {
            if (!have) start = cur;
            break;
        }
        if (e[0] == 0x00) return -ENOENT;
        if (e[0] != 0xE5 && (e[11] & 0x3Fu) == 0x0Fu) {
            if (!have) { start = cur; have = true; }
        } else {
            have = false;
        }
        result = cur_advance(vol, &cur);
        if (result < 0) return result;
    }
    cur = start;
    for (;;) {
        int result = ent_mark_deleted(vol, &cur);
        if (result < 0) return result;
        if (cur.cluster == info->dir_cluster && cur.pos == info->dir_pos) return 0;
        result = cur_advance(vol, &cur);
        if (result < 0) return result;
    }
}

/* ---- resolucao de caminhos ---- */

/* Separa `path` em pai + folha. `parent` = entrada do diretorio pai (tem que
 * ser diretorio), `pp` = copia do caminho do pai, `leaf` = ultimo componente.
 * 0 se a folha existe (*info), -ENOENT se nao existe (pai/folha validos). */
static int resolve_leaf(struct fat32_vol* vol, const char* path,
                        struct fat32_info* parent, char* pp, char* leaf,
                        struct fat32_info* info) {
    size_t end = strlen(path);
    while (end > 0 && path[end - 1] == '/') end--;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') start--;
    size_t leaf_len = end - start;
    if (leaf_len == 0) return -EINVAL;               /* raiz / vazio */
    if (leaf_len > FAT32_NAME_MAX) return -ENAMETOOLONG;
    size_t plen = start;
    while (plen > 0 && path[plen - 1] == '/') plen--;
    if (plen >= FAT32_PATH_BUF) return -ENAMETOOLONG;
    memcpy(leaf, path + start, leaf_len);
    leaf[leaf_len] = '\0';
    memcpy(pp, path, plen);
    pp[plen] = '\0';
    if ((leaf_len == 1 && leaf[0] == '.') ||
        (leaf_len == 2 && leaf[0] == '.' && leaf[1] == '.'))
        return -EINVAL;
    int result = fat32_lookup(vol, pp, parent);
    if (result < 0) return result;
    if (!(parent->attr & FAT32_ATTR_DIR)) return -ENOTDIR;
    return dir_find(vol, parent->first_cluster, leaf, info);
}

static int dir_is_empty(struct fat32_vol* vol, const struct fat32_info* dir) {
    if (dir->first_cluster == 0) return -EIO;
    struct fat32_dircur cur;
    struct fat32_info tmp;
    cur_init(&cur, dir->first_cluster);
    int result = dir_next(vol, &cur, &tmp);
    if (result < 0) return result;
    return result == 0 ? 0 : -ENOTEMPTY;
}

/* Aponta o ".." do diretorio `dir_cluster` para `parent_first` (0 = raiz). */
static int set_dotdot(struct fat32_vol* vol, uint32_t dir_cluster,
                      uint32_t parent_first) {
    struct blkbuf* buf;
    int result = bread(vol->dev, cluster_lba(vol, dir_cluster), &buf);
    if (result < 0) return result;
    uint8_t* e = buf->data + 32;
    if (e[0] != '.' || e[1] != '.') {
        brelse(buf);
        return -EIO;
    }
    uint32_t value = parent_first == vol->root_cluster ? 0 : parent_first;
    put16(e + 20, value >> 16);
    put16(e + 26, value);
    bdirty(buf);
    brelse(buf);
    return 0;
}

/* `cluster` e algum componente (ou o proprio) do caminho `pp`? */
static int path_has_cluster(struct fat32_vol* vol, const char* pp,
                            uint32_t cluster) {
    char buf[FAT32_PATH_BUF];
    size_t len = strlen(pp);
    for (size_t i = 1; i <= len; i++) {
        if (pp[i] != '/' && pp[i] != '\0') continue;
        if (pp[i - 1] == '/') continue;
        struct fat32_info info;
        memcpy(buf, pp, i);
        buf[i] = '\0';
        int result = fat32_lookup(vol, buf, &info);
        if (result < 0) return result;
        if (info.first_cluster == cluster) return 1;
    }
    return 0;
}

/* ---- operacoes ---- */

static int create_locked(struct fat32_vol* vol, const char* path, uint8_t attr,
                         struct fat32_info* out) {
    struct fat32_info parent, found;
    char pp[FAT32_PATH_BUF], leaf[FAT32_NAME_MAX + 1];
    int result = resolve_leaf(vol, path, &parent, pp, leaf, &found);
    if (result == 0) return -EEXIST;
    if (result != -ENOENT) return result;
    uint16_t units[255];
    result = name_units(leaf, units);
    if (result < 0) return result;
    uint16_t date, time;
    fat_now(&date, &time);

    uint32_t cluster = 0;
    if (attr & FAT32_ATTR_DIR) {
        result = chain_grow(vol, 0, true, &cluster);     /* cluster zerado */
        if (result < 0) return result;
        struct blkbuf* buf;
        result = bread(vol->dev, cluster_lba(vol, cluster), &buf);
        if (result < 0) {
            (void)cluster_free_chain(vol, cluster);
            return result;
        }
        uint32_t up = parent.first_cluster == vol->root_cluster
                          ? 0 : parent.first_cluster;
        for (uint32_t i = 0; i < 2u; i++) {
            uint8_t* e = buf->data + i * 32u;
            uint32_t target = i == 0 ? cluster : up;
            memset(e, ' ', 11);
            e[0] = '.';
            if (i == 1) e[1] = '.';
            e[11] = FAT32_ATTR_DIR;
            put16(e + 14, time);
            put16(e + 16, date);
            put16(e + 18, date);
            put16(e + 20, target >> 16);
            put16(e + 22, time);
            put16(e + 24, date);
            put16(e + 26, target);
            put32(e + 28, 0);
        }
        bdirty(buf);
        brelse(buf);
    }
    result = dir_add_entry(vol, parent.first_cluster, leaf, attr, cluster, 0,
                           date, time, out);
    if (result < 0 && cluster) (void)cluster_free_chain(vol, cluster);
    return result;
}

int fat32_create(struct fat32_vol* vol, const char* path, struct fat32_info* out) {
    if (!vol || !path) return -EINVAL;
    if (vol->readonly) return -EROFS;
    int result = dirop_enter(vol);
    if (result < 0) return result;
    result = create_locked(vol, path, FAT32_ATTR_ARCH, out);
    dirop_leave(vol);
    return result;
}

int fat32_mkdir(struct fat32_vol* vol, const char* path) {
    if (!vol || !path) return -EINVAL;
    if (vol->readonly) return -EROFS;
    int result = dirop_enter(vol);
    if (result < 0) return result;
    result = create_locked(vol, path, FAT32_ATTR_DIR, NULL);
    dirop_leave(vol);
    return result;
}

static int remove_locked(struct fat32_vol* vol, const char* path, bool want_dir) {
    struct fat32_info parent, info;
    char pp[FAT32_PATH_BUF], leaf[FAT32_NAME_MAX + 1];
    int result = resolve_leaf(vol, path, &parent, pp, leaf, &info);
    if (result < 0) return result;
    bool is_dir = (info.attr & FAT32_ATTR_DIR) != 0;
    if (want_dir && !is_dir) return -ENOTDIR;
    if (!want_dir && is_dir) return -EISDIR;
    if (info.attr & FAT32_ATTR_RO) return -EACCES;
    if (info.dir_cluster == 0) return -EBUSY;
    if (entry_is_open(vol, &info)) return -EBUSY;
    if (want_dir) {
        result = dir_is_empty(vol, &info);
        if (result < 0) return result;
    }
    result = entry_delete(vol, parent.first_cluster, &info);
    if (result < 0) return result;
    if (info.first_cluster) return cluster_free_chain(vol, info.first_cluster);
    return 0;
}

int fat32_unlink(struct fat32_vol* vol, const char* path) {
    if (!vol || !path) return -EINVAL;
    if (vol->readonly) return -EROFS;
    int result = dirop_enter(vol);
    if (result < 0) return result;
    result = remove_locked(vol, path, false);
    dirop_leave(vol);
    return result;
}

int fat32_rmdir(struct fat32_vol* vol, const char* path) {
    if (!vol || !path) return -EINVAL;
    if (vol->readonly) return -EROFS;
    int result = dirop_enter(vol);
    if (result < 0) return result;
    result = remove_locked(vol, path, true);
    dirop_leave(vol);
    return result;
}

static int rename_locked(struct fat32_vol* vol, const char* old_path,
                         const char* new_path) {
    struct fat32_info op, oi, np, ni, created;
    char opp[FAT32_PATH_BUF], npp[FAT32_PATH_BUF];
    char oleaf[FAT32_NAME_MAX + 1], nleaf[FAT32_NAME_MAX + 1];
    int result = resolve_leaf(vol, old_path, &op, opp, oleaf, &oi);
    if (result < 0) return result;
    if (oi.dir_cluster == 0) return -EBUSY;                  /* raiz */
    result = resolve_leaf(vol, new_path, &np, npp, nleaf, &ni);
    bool found = result == 0;
    if (result < 0 && result != -ENOENT) return result;
    uint16_t units[255];
    result = name_units(nleaf, units);                       /* valida antes */
    if (result < 0) return result;

    bool old_dir = (oi.attr & FAT32_ATTR_DIR) != 0;
    bool same = found && ni.dir_cluster == oi.dir_cluster && ni.dir_pos == oi.dir_pos;
    if (same && str_same(oleaf, nleaf)) return 0;
    if (entry_is_open(vol, &oi)) return -EBUSY;
    if (old_dir) {
        if (np.first_cluster == oi.first_cluster) return -EINVAL;
        result = path_has_cluster(vol, npp, oi.first_cluster);
        if (result < 0) return result;
        if (result == 1) return -EINVAL;
    }
    if (found && !same) {
        bool new_dir = (ni.attr & FAT32_ATTR_DIR) != 0;
        if (old_dir && !new_dir) return -ENOTDIR;
        if (!old_dir && new_dir) return -EISDIR;
        if (ni.attr & FAT32_ATTR_RO) return -EACCES;
        if (ni.dir_cluster == 0 || entry_is_open(vol, &ni)) return -EBUSY;
        if (new_dir) {
            result = dir_is_empty(vol, &ni);
            if (result < 0) return result;
        }
        /* libera o nome do destino; a cadeia so e liberada no fim */
        result = entry_delete(vol, np.first_cluster, &ni);
        if (result < 0) return result;
    } else if (same) {
        /* so muda a caixa/alias do mesmo arquivo: apaga e recria */
        result = entry_delete(vol, op.first_cluster, &oi);
        if (result < 0) return result;
    }
    result = dir_add_entry(vol, np.first_cluster, nleaf, oi.attr,
                           oi.first_cluster, oi.size, oi.wdate, oi.wtime,
                           &created);
    if (result < 0) return result;           /* (destino ja removido: sem undo) */
    if (!same) {
        result = entry_delete(vol, op.first_cluster, &oi);
        if (result < 0) return result;
    }
    if (old_dir && np.first_cluster != op.first_cluster) {
        result = set_dotdot(vol, oi.first_cluster, np.first_cluster);
        if (result < 0) return result;
    }
    if (found && !same && ni.first_cluster)
        return cluster_free_chain(vol, ni.first_cluster);
    return 0;
}

int fat32_rename(struct fat32_vol* vol, const char* old_path,
                 const char* new_path) {
    if (!vol || !old_path || !new_path) return -EINVAL;
    if (vol->readonly) return -EROFS;
    int result = dirop_enter(vol);
    if (result < 0) return result;
    result = rename_locked(vol, old_path, new_path);
    dirop_leave(vol);
    return result;
}

/* ------------------------------------------------------- cola com o VFS (4.1) */

static struct vnode* fsop_open(void* fs, const char* rel, int flags, int* err) {
    return fat32_vnode_open((struct fat32_vol*)fs, rel, flags, err);
}
static int fsop_unlink(void* fs, const char* rel) { return fat32_unlink((struct fat32_vol*)fs, rel); }
static int fsop_mkdir(void* fs, const char* rel) { return fat32_mkdir((struct fat32_vol*)fs, rel); }
static int fsop_rmdir(void* fs, const char* rel) { return fat32_rmdir((struct fat32_vol*)fs, rel); }
static int fsop_rename(void* fs, const char* a, const char* b) {
    return fat32_rename((struct fat32_vol*)fs, a, b);
}
static int fsop_sync(void* fs) { return fat32_sync((struct fat32_vol*)fs); }
static int fsop_unmount(void* fs) { return fat32_unmount((struct fat32_vol*)fs); }

static const struct vfs_fsops g_fat32_fsops = {
    .open = fsop_open, .unlink = fsop_unlink, .mkdir = fsop_mkdir,
    .rmdir = fsop_rmdir, .rename = fsop_rename, .sync = fsop_sync,
    .unmount = fsop_unmount,
};

int fat32_vfs_mount(struct blkdev* dev, const char* path) {
    struct fat32_vol* vol;
    int result = fat32_mount(dev, &vol);
    if (result < 0) return result;
    result = vfs_mount_fs(path, &g_fat32_fsops, vol);
    if (result < 0) (void)fat32_unmount(vol);
    return result;
}
