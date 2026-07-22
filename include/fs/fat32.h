/*
 * fs/fat32.h - FAT32 driver (minimal, enough to read /boot)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _FS_FAT32_H
#define _FS_FAT32_H

#include <fs/vfs.h>

struct fat32_boot_sector {
    uint8_t jmp[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint32_t root_cluster;
} __attribute__((packed));

int fat32_mount(struct vnode* device, struct vnode** root);

#endif /* _FS_FAT32_H */
