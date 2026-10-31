/*
 * drivers/block/ramdisk.c - Memory-backed block device
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#include <drivers/block/blk.h>
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>

struct ramdisk {
    uint8_t* mem;
    bool owns_mem;
};

static int ram_read(struct blkdev* dev, uint64_t lba, uint32_t count,
                    void* buf) {
    struct ramdisk* rd = dev->priv;
    memcpy(buf, rd->mem + lba * BLK_SECTOR_SIZE,
           (size_t)count * BLK_SECTOR_SIZE);
    return 0;
}

static int ram_write(struct blkdev* dev, uint64_t lba, uint32_t count,
                     const void* buf) {
    struct ramdisk* rd = dev->priv;
    memcpy(rd->mem + lba * BLK_SECTOR_SIZE, buf,
           (size_t)count * BLK_SECTOR_SIZE);
    return 0;
}

static const struct blkdev_ops g_ram_ops = {ram_read, ram_write, NULL};

struct blkdev* ramdisk_create(const char* name, void* mem, size_t size,
                              bool readonly) {
    if (!name || !mem || size < BLK_SECTOR_SIZE ||
        strlen(name) >= BLK_NAME_MAX)
        return NULL;
    struct blkdev* dev = kzalloc(sizeof(*dev));
    struct ramdisk* rd = kzalloc(sizeof(*rd));
    if (!dev || !rd) {
        kfree(dev);
        kfree(rd);
        return NULL;
    }
    rd->mem = mem;
    strcpy(dev->name, name);
    dev->sectors = size / BLK_SECTOR_SIZE;
    dev->readonly = readonly;
    dev->ops = &g_ram_ops;
    dev->priv = rd;
    dev->owned = true;
    if (blk_register(dev) < 0) {
        kfree(dev);
        kfree(rd);
        return NULL;
    }
    return dev;
}

struct blkdev* ramdisk_alloc(const char* name, size_t size) {
    size &= ~(size_t)(BLK_SECTOR_SIZE - 1);
    if (!size) return NULL;
    void* mem = kzalloc(size);
    if (!mem) return NULL;
    struct blkdev* dev = ramdisk_create(name, mem, size, false);
    if (!dev) {
        kfree(mem);
        return NULL;
    }
    ((struct ramdisk*)dev->priv)->owns_mem = true;
    return dev;
}

void ramdisk_destroy(struct blkdev* dev) {
    if (!dev || dev->ops != &g_ram_ops) return;
    struct ramdisk* rd = dev->priv;
    uint8_t* mem = rd->mem;
    bool owns = rd->owns_mem;
    if (blk_unregister(dev) < 0) return;   /* ocupado: mantem tudo vivo */
    kfree(rd);
    if (owns) kfree(mem);
}
