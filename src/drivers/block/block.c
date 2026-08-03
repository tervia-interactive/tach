#include <drivers/block/block.h>
#include <kernel/errno.h>
#include <kernel/string.h>

#define BLOCK_MAX_DEVICES 8
static struct block_device* g_devices[BLOCK_MAX_DEVICES];
static size_t g_device_count;

int block_register(struct block_device* device) {
    if (!device || !device->ops || !device->ops->read ||
        !device->sector_size || g_device_count >= BLOCK_MAX_DEVICES)
        return -EINVAL;
    for (size_t i = 0; i < g_device_count; i++)
        if (!strcmp(g_devices[i]->name, device->name)) return -EEXIST;
    g_devices[g_device_count++] = device;
    return 0;
}
size_t block_device_count(void) { return g_device_count; }
struct block_device* block_device_at(size_t index) {
    return index < g_device_count ? g_devices[index] : NULL;
}
struct block_device* block_find(const char* name) {
    if (!name) return NULL;
    for (size_t i = 0; i < g_device_count; i++)
        if (!strcmp(g_devices[i]->name, name)) return g_devices[i];
    return NULL;
}
int block_read(struct block_device* device, uint64_t lba,
               uint32_t count, void* buffer) {
    if (!device || !buffer || !count || !device->ops || !device->ops->read)
        return -EINVAL;
    if (device->sector_count &&
        (lba >= device->sector_count || count > device->sector_count - lba))
        return -EIO;
    return device->ops->read(device, lba, count, buffer);
}
int block_write(struct block_device* device, uint64_t lba,
                uint32_t count, const void* buffer) {
    if (!device || !buffer || !count || !device->ops || !device->ops->write)
        return -EROFS;
    if (device->sector_count &&
        (lba >= device->sector_count || count > device->sector_count - lba))
        return -EIO;
    return device->ops->write(device, lba, count, buffer);
}
int block_sync(struct block_device* device) {
    if (!device) return -EINVAL;
    return device->ops && device->ops->sync ? device->ops->sync(device) : 0;
}
