/*
 * drivers/block/blk.c - Block device registry and write-back sector cache
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#include <drivers/block/blk.h>
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/kheap.h>

#define BCACHE_BUFFERS 128
#define BCACHE_BUCKETS 64
#define BLK_READ_RUN   64u      /* setores por leitura direta (read-around) */

static spinlock_t g_blk_lock = SPINLOCK_INIT;
static struct blkdev* g_devs[BLK_MAX_DEVICES];

static struct blkbuf* g_pool;
static struct blkbuf* g_buckets[BCACHE_BUCKETS];
static struct blkbuf* g_lru_head;   /* mais recente */
static struct blkbuf* g_lru_tail;   /* menos recente */
static bool g_ready;

/* ------------------------------------------------------------------ util */

static struct blkdev* root_of(struct blkdev* dev, uint64_t lba,
                              uint64_t* absolute) {
    uint64_t abs = lba;
    while (dev->parent) {
        abs += dev->start;
        dev = dev->parent;
    }
    *absolute = abs;
    return dev;
}

static bool range_ok(const struct blkdev* dev, uint64_t lba, uint64_t count) {
    return count > 0 && lba <= dev->sectors && count <= dev->sectors - lba;
}

static unsigned hash_of(const struct blkdev* dev, uint64_t lba) {
    uint64_t x = (uint64_t)(uintptr_t)dev >> 4;
    x ^= lba * 0x9E3779B1u;
    x ^= x >> 17;
    return (unsigned)(x % BCACHE_BUCKETS);
}

static int drv_read(struct blkdev* root, uint64_t lba, uint32_t count,
                    void* buf) {
    if (!root->ops || !root->ops->read) return -ENODEV;
    if (!range_ok(root, lba, count)) return -EINVAL;
    return root->ops->read(root, lba, count, buf);
}

static int drv_write(struct blkdev* root, uint64_t lba, uint32_t count,
                     const void* buf) {
    if (root->readonly) return -EROFS;
    if (!root->ops || !root->ops->write) return -EROFS;
    if (!range_ok(root, lba, count)) return -EINVAL;
    return root->ops->write(root, lba, count, buf);
}

/* ------------------------------------------------------- LRU / hash list */

static void lru_remove(struct blkbuf* b) {
    if (b->lru_prev) b->lru_prev->lru_next = b->lru_next;
    else g_lru_head = b->lru_next;
    if (b->lru_next) b->lru_next->lru_prev = b->lru_prev;
    else g_lru_tail = b->lru_prev;
    b->lru_prev = b->lru_next = NULL;
}

static void lru_push_front(struct blkbuf* b) {
    b->lru_prev = NULL;
    b->lru_next = g_lru_head;
    if (g_lru_head) g_lru_head->lru_prev = b;
    g_lru_head = b;
    if (!g_lru_tail) g_lru_tail = b;
}

static void lru_touch(struct blkbuf* b) {
    if (g_lru_head == b) return;
    lru_remove(b);
    lru_push_front(b);
}

static void hash_remove(struct blkbuf* b) {
    if (!b->dev) return;
    struct blkbuf** link = &g_buckets[hash_of(b->dev, b->lba)];
    while (*link) {
        if (*link == b) {
            *link = b->hnext;
            break;
        }
        link = &(*link)->hnext;
    }
    b->hnext = NULL;
}

static struct blkbuf* lookup(struct blkdev* root, uint64_t lba) {
    for (struct blkbuf* b = g_buckets[hash_of(root, lba)]; b; b = b->hnext) {
        if (b->dev == root && b->lba == lba) return b;
    }
    return NULL;
}

static int write_back(struct blkbuf* b) {
    if (!b->dirty) return 0;
    int result = drv_write(b->dev, b->lba, 1, b->data);
    if (result == 0) b->dirty = false;
    return result;
}

/* Escolhe o buffer menos recente sem referencias, escrevendo-o se sujo. */
static int take_victim(struct blkbuf** out) {
    for (struct blkbuf* b = g_lru_tail; b; b = b->lru_prev) {
        if (b->refs) continue;
        int result = write_back(b);
        if (result < 0) return result;
        hash_remove(b);
        b->dev = NULL;
        *out = b;
        return 0;
    }
    return -EBUSY;
}

/* Chamada com g_blk_lock tomado. Devolve o buffer com uma referencia. */
static int get_buf(struct blkdev* root, uint64_t abs, bool read,
                   struct blkbuf** out) {
    struct blkbuf* b = lookup(root, abs);
    if (b) {
        b->refs++;
        lru_touch(b);
        *out = b;
        return 0;
    }
    int result = take_victim(&b);
    if (result < 0) return result;
    if (read) {
        result = drv_read(root, abs, 1, b->data);
        if (result < 0) return result;          /* buffer segue livre */
    } else {
        memset(b->data, 0, BLK_SECTOR_SIZE);
    }
    b->dev = root;
    b->lba = abs;
    b->dirty = false;
    b->refs = 1;
    unsigned h = hash_of(root, abs);
    b->hnext = g_buckets[h];
    g_buckets[h] = b;
    lru_touch(b);
    *out = b;
    return 0;
}

/* --------------------------------------------------------------- registro */

void blk_init(void) {
    spinlock_lock(&g_blk_lock);
    if (g_ready || g_pool) {
        spinlock_unlock(&g_blk_lock);
        return;
    }
    g_pool = kzalloc(sizeof(struct blkbuf) * BCACHE_BUFFERS);
    if (g_pool) {
        for (size_t i = 0; i < BCACHE_BUFFERS; i++) lru_push_front(&g_pool[i]);
        g_ready = true;
    }   /* sem memoria: segue sem cache, com I/O direto (write-through) */
    spinlock_unlock(&g_blk_lock);
}

int blk_register(struct blkdev* dev) {
    if (!dev || !dev->name[0]) return -EINVAL;
    if (!dev->parent && (!dev->ops || !dev->ops->read)) return -EINVAL;
    if (dev->parent && dev->start + dev->sectors > dev->parent->sectors)
        return -EINVAL;
    int result = -ENOSPC;
    spinlock_lock(&g_blk_lock);
    for (size_t i = 0; i < BLK_MAX_DEVICES; i++) {
        if (g_devs[i] && strcmp(g_devs[i]->name, dev->name) == 0) {
            result = -EEXIST;
            goto out;
        }
    }
    for (size_t i = 0; i < BLK_MAX_DEVICES; i++) {
        if (!g_devs[i]) {
            g_devs[i] = dev;
            result = 0;
            break;
        }
    }
out:
    spinlock_unlock(&g_blk_lock);
    return result;
}

struct blkdev* blk_find(const char* name) {
    if (!name) return NULL;
    struct blkdev* found = NULL;
    spinlock_lock(&g_blk_lock);
    for (size_t i = 0; i < BLK_MAX_DEVICES; i++) {
        if (g_devs[i] && strcmp(g_devs[i]->name, name) == 0) {
            found = g_devs[i];
            break;
        }
    }
    spinlock_unlock(&g_blk_lock);
    return found;
}

struct blkdev* blk_get(size_t index) {
    struct blkdev* found = NULL;
    spinlock_lock(&g_blk_lock);
    for (size_t i = 0, seen = 0; i < BLK_MAX_DEVICES; i++) {
        if (!g_devs[i]) continue;
        if (seen++ == index) {
            found = g_devs[i];
            break;
        }
    }
    spinlock_unlock(&g_blk_lock);
    return found;
}

size_t blk_count(void) {
    size_t n = 0;
    spinlock_lock(&g_blk_lock);
    for (size_t i = 0; i < BLK_MAX_DEVICES; i++) if (g_devs[i]) n++;
    spinlock_unlock(&g_blk_lock);
    return n;
}

/* g_blk_lock tomado. */
static int flush_root_locked(struct blkdev* root) {
    int first_error = 0;
    if (g_ready) {
        for (size_t i = 0; i < BCACHE_BUFFERS; i++) {
            struct blkbuf* b = &g_pool[i];
            if (b->dev != root || !b->dirty) continue;
            int result = write_back(b);
            if (result < 0 && !first_error) first_error = result;
        }
    }
    if (!first_error && root->ops && root->ops->flush)
        first_error = root->ops->flush(root);
    return first_error;
}

int blk_unregister(struct blkdev* dev) {
    if (!dev) return -EINVAL;
    struct blkdev* children[BLK_MAX_DEVICES];
    size_t child_count = 0;
    spinlock_lock(&g_blk_lock);
    bool present = false;
    for (size_t i = 0; i < BLK_MAX_DEVICES; i++) {
        if (g_devs[i] == dev) present = true;
        else if (g_devs[i] && g_devs[i]->parent == dev)
            children[child_count++] = g_devs[i];
    }
    spinlock_unlock(&g_blk_lock);
    if (!present) return -ENODEV;
    for (size_t i = 0; i < child_count; i++) {
        int result = blk_unregister(children[i]);
        if (result < 0) return result;
    }

    spinlock_lock(&g_blk_lock);
    int result = 0;
    if (!dev->parent) {
        result = flush_root_locked(dev);
        if (g_ready) {
            for (size_t i = 0; i < BCACHE_BUFFERS && result == 0; i++) {
                if (g_pool[i].dev == dev && g_pool[i].refs) result = -EBUSY;
            }
            for (size_t i = 0; i < BCACHE_BUFFERS && result == 0; i++) {
                struct blkbuf* b = &g_pool[i];
                if (b->dev != dev) continue;
                hash_remove(b);
                b->dev = NULL;
                b->dirty = false;
            }
        }
    }
    if (result == 0) {
        for (size_t i = 0; i < BLK_MAX_DEVICES; i++)
            if (g_devs[i] == dev) g_devs[i] = NULL;
    }
    spinlock_unlock(&g_blk_lock);
    if (result == 0 && dev->owned) kfree(dev);
    return result;
}

/* ------------------------------------------------------------ I/O setores */

int blk_read(struct blkdev* dev, uint64_t lba, uint32_t count, void* buf) {
    if (!dev || !buf) return -EINVAL;
    if (!range_ok(dev, lba, count)) return -EINVAL;
    uint64_t abs;
    struct blkdev* root = root_of(dev, lba, &abs);
    uint8_t* out = buf;
    int result = 0;

    spinlock_lock(&g_blk_lock);
    if (!g_ready) {
        result = drv_read(root, abs, count, out);
        spinlock_unlock(&g_blk_lock);
        return result;
    }
    while (count && result == 0) {
        struct blkbuf* cached = lookup(root, abs);
        uint32_t step = 1;
        if (cached) {
            memcpy(out, cached->data, BLK_SECTOR_SIZE);
            lru_touch(cached);
        } else {
            /* Leitura direta ("read-around") dos setores seguidos que ainda
             * nao estao em cache, numa unica chamada ao driver. */
            while (step < count && step < BLK_READ_RUN &&
                   !lookup(root, abs + step))
                step++;
            result = drv_read(root, abs, step, out);
        }
        out += (size_t)step * BLK_SECTOR_SIZE;
        abs += step;
        count -= step;
    }
    spinlock_unlock(&g_blk_lock);
    return result;
}

int blk_write(struct blkdev* dev, uint64_t lba, uint32_t count,
              const void* buf) {
    if (!dev || !buf) return -EINVAL;
    if (!range_ok(dev, lba, count)) return -EINVAL;
    uint64_t abs;
    struct blkdev* root = root_of(dev, lba, &abs);
    if (dev->readonly || root->readonly) return -EROFS;
    const uint8_t* in = buf;
    int result = 0;

    spinlock_lock(&g_blk_lock);
    if (!g_ready) {
        result = drv_write(root, abs, count, in);
        spinlock_unlock(&g_blk_lock);
        return result;
    }
    while (count && result == 0) {
        struct blkbuf* b;
        result = get_buf(root, abs, false, &b);
        if (result < 0) break;
        memcpy(b->data, in, BLK_SECTOR_SIZE);
        b->dirty = true;
        b->refs--;
        in += BLK_SECTOR_SIZE;
        abs++;
        count--;
    }
    spinlock_unlock(&g_blk_lock);
    return result;
}

int blk_flush(struct blkdev* dev) {
    if (!dev) return -EINVAL;
    uint64_t unused;
    struct blkdev* root = root_of(dev, 0, &unused);
    spinlock_lock(&g_blk_lock);
    int result = flush_root_locked(root);
    spinlock_unlock(&g_blk_lock);
    return result;
}

int blk_sync_all(void) {
    int first_error = 0;
    spinlock_lock(&g_blk_lock);
    for (size_t i = 0; i < BLK_MAX_DEVICES; i++) {
        if (!g_devs[i] || g_devs[i]->parent) continue;
        int result = flush_root_locked(g_devs[i]);
        if (result < 0 && !first_error) first_error = result;
    }
    spinlock_unlock(&g_blk_lock);
    return first_error;
}

/* -------------------------------------------------------------- buffers */

static int buf_get(struct blkdev* dev, uint64_t lba, bool read,
                   struct blkbuf** out) {
    if (!dev || !out) return -EINVAL;
    if (!range_ok(dev, lba, 1)) return -EINVAL;
    if (!g_ready) return -ENOMEM;
    uint64_t abs;
    struct blkdev* root = root_of(dev, lba, &abs);
    if (!read && (dev->readonly || root->readonly)) return -EROFS;
    spinlock_lock(&g_blk_lock);
    int result = get_buf(root, abs, read, out);
    if (result == 0 && !read) {
        /* bnew: conteudo anterior descartado; o setor passa a ser sujo para
         * que o cache nunca fique diferente do que o chamador escreveu. */
        memset((*out)->data, 0, BLK_SECTOR_SIZE);
        (*out)->dirty = true;
    }
    spinlock_unlock(&g_blk_lock);
    return result;
}

int bread(struct blkdev* dev, uint64_t lba, struct blkbuf** out) {
    return buf_get(dev, lba, true, out);
}

int bnew(struct blkdev* dev, uint64_t lba, struct blkbuf** out) {
    return buf_get(dev, lba, false, out);
}

void bdirty(struct blkbuf* buf) {
    if (!buf) return;
    spinlock_lock(&g_blk_lock);
    buf->dirty = true;
    spinlock_unlock(&g_blk_lock);
}

void brelse(struct blkbuf* buf) {
    if (!buf) return;
    spinlock_lock(&g_blk_lock);
    if (buf->refs) buf->refs--;
    spinlock_unlock(&g_blk_lock);
}

/* ------------------------------------------------------------ I/O bytes */

int blk_read_bytes(struct blkdev* dev, uint64_t offset, void* buf,
                   size_t len) {
    if (!dev || (!buf && len)) return -EINVAL;
    uint64_t size = dev->sectors * BLK_SECTOR_SIZE;
    if (offset > size || len > size - offset) return -EINVAL;
    uint8_t* out = buf;
    while (len) {
        struct blkbuf* b;
        int result = bread(dev, offset / BLK_SECTOR_SIZE, &b);
        if (result < 0) return result;
        size_t within = (size_t)(offset % BLK_SECTOR_SIZE);
        size_t chunk = BLK_SECTOR_SIZE - within;
        if (chunk > len) chunk = len;
        memcpy(out, b->data + within, chunk);
        brelse(b);
        out += chunk;
        offset += chunk;
        len -= chunk;
    }
    return 0;
}

int blk_write_bytes(struct blkdev* dev, uint64_t offset, const void* buf,
                    size_t len) {
    if (!dev || (!buf && len)) return -EINVAL;
    uint64_t size = dev->sectors * BLK_SECTOR_SIZE;
    if (offset > size || len > size - offset) return -EINVAL;
    uint64_t abs;
    if (dev->readonly || root_of(dev, 0, &abs)->readonly) return -EROFS;
    const uint8_t* in = buf;
    while (len) {
        size_t within = (size_t)(offset % BLK_SECTOR_SIZE);
        size_t chunk = BLK_SECTOR_SIZE - within;
        if (chunk > len) chunk = len;
        struct blkbuf* b;
        int result = chunk == BLK_SECTOR_SIZE
            ? bnew(dev, offset / BLK_SECTOR_SIZE, &b)
            : bread(dev, offset / BLK_SECTOR_SIZE, &b);
        if (result < 0) return result;
        memcpy(b->data + within, in, chunk);
        bdirty(b);
        brelse(b);
        in += chunk;
        offset += chunk;
        len -= chunk;
    }
    return 0;
}
