#ifndef _DRIVERS_BLOCK_BLOCK_H
#define _DRIVERS_BLOCK_BLOCK_H

#include <kernel/types.h>

struct block_device;

struct block_device_ops {
    int (*read)(struct block_device* device, uint64_t lba,
                uint32_t count, void* buffer);
    int (*write)(struct block_device* device, uint64_t lba,
                 uint32_t count, const void* buffer);
    int (*sync)(struct block_device* device);
};

struct block_device {
    char name[16];
    uint32_t sector_size;
    uint64_t sector_count;
    void* data;
    const struct block_device_ops* ops;
};

int block_register(struct block_device* device);
size_t block_device_count(void);
struct block_device* block_device_at(size_t index);
struct block_device* block_find(const char* name);
int block_read(struct block_device* device, uint64_t lba,
               uint32_t count, void* buffer);
int block_write(struct block_device* device, uint64_t lba,
                uint32_t count, const void* buffer);
int block_sync(struct block_device* device);

#endif
