#ifndef _FS_FAT32_H
#define _FS_FAT32_H

#include <fs/vfs.h>
#include <drivers/block/block.h>

struct fat32_boot_sector {
    uint8_t jmp[3]; char oem_name[8]; uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster; uint16_t reserved_sectors; uint8_t num_fats;
    uint16_t root_entries, total_sectors_16; uint8_t media_type;
    uint16_t fat_size_16, sectors_per_track, heads;
    uint32_t hidden_sectors, total_sectors_32, fat_size_32;
    uint16_t extended_flags, fs_version;
    uint32_t root_cluster; uint16_t fs_info, backup_boot;
} __attribute__((packed));

int fat32_mount(struct block_device* device, struct vnode** root);

#endif
