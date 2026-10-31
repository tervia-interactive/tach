/*
 * fs/fat32.c - FAT32 somente leitura (BPB, cadeia da FAT, diretorios, LFN)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Todo acesso a disco passa pela camada de bloco (bread/blk_read). O unico
 * estado mutavel e o contador de vnodes abertos e os caches de percurso de
 * cada no (ultimo cluster lido, cursor de readdir), protegidos por um
 * spinlock curto; o spinlock nunca e mantido durante E/S.
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

struct fat32_vol {
    struct blkdev* dev;
    uint64_t fat_lba;           /* inicio da FAT ativa */
    uint64_t data_lba;          /* setor do cluster 2 */
    uint32_t clusters;          /* clusters de dados (numerados 2..clusters+1) */
    uint32_t root_cluster;
    uint32_t spc;               /* setores por cluster (potencia de 2) */
    uint32_t spc_shift;
    uint32_t entries_per_cluster;
    uint32_t open_nodes;        /* vnodes vivos (protegido por `lock`) */
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

/* ------------------------------------------------------------------ mount */

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
    vol->fat_lba = (uint64_t)reserved + (uint64_t)active * fat_size;
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
    klog_info("fat32", "%s: %u clusters de %u bytes, raiz no cluster %u",
              dev->name, (unsigned)clusters, (unsigned)(spc * BLK_SECTOR_SIZE),
              (unsigned)root);
    *out = vol;
    return 0;
}

int fat32_unmount(struct fat32_vol* vol) {
    if (!vol) return -EINVAL;
    spinlock_lock(&vol->lock);
    uint32_t open_nodes = vol->open_nodes;
    spinlock_unlock(&vol->lock);
    if (open_nodes) return -EBUSY;
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

/* ----------------------------------------------------------------- vnode */

#define FAT32_VNODE_PATH_MAX sizeof(((struct vnode*)0)->path)

struct fat32_vnode {
    struct vnode vn;            /* primeiro membro: cast direto */
    struct fat32_node node;
    struct fat32_vol* vol;
    size_t opens;               /* descritores apontando para este vnode */
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
    (void)vn;
    (void)buffer;
    (void)count;
    (void)offset;
    return -EROFS;
}

static void fv_dup(struct vnode* vn) {
    struct fat32_vnode* fv = fv_of(vn);
    if (!fv) return;
    spinlock_lock(&fv->vol->lock);
    fv->opens++;
    spinlock_unlock(&fv->vol->lock);
}

static void fv_release(struct vnode* vn) {
    struct fat32_vnode* fv = fv_of(vn);
    if (!fv) return;
    struct fat32_vol* vol = fv->vol;
    bool last = false;
    spinlock_lock(&vol->lock);
    if (fv->opens && --fv->opens == 0) {
        vol->open_nodes--;
        last = true;
    }
    spinlock_unlock(&vol->lock);
    if (last) kfree(fv);
}

static struct vnode_ops g_fat32_ops = {
    .open = NULL, .close = NULL, .read = fv_read, .write = fv_write,
    .readdir = NULL, .dup = fv_dup, .release = fv_release,
};

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
    if (result == -ENOENT && (flags & O_CREAT)) result = -EROFS;
    if (result < 0) {
        *error = result;
        return NULL;
    }
    if ((flags & O_ACCMODE) != O_RDONLY || (flags & (O_TRUNC | O_APPEND))) {
        *error = (info.attr & FAT32_ATTR_DIR) ? -EISDIR : -EROFS;
        return NULL;
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
    vol->open_nodes++;
    spinlock_unlock(&vol->lock);
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
