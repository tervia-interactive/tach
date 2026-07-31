/*
 * boot/multiboot1.h - Multiboot 1 specification structures
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _BOOT_MULTIBOOT1_H
#define _BOOT_MULTIBOOT1_H

#include <kernel/types.h>

#define MULTIBOOT1_MAGIC 0x1BADB002
#define MULTIBOOT1_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT1_INFO_MEMORY  (1u << 0)
#define MULTIBOOT1_INFO_MODULES (1u << 3)
#define MULTIBOOT1_INFO_MMAP    (1u << 6)

struct multiboot1_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
} __attribute__((packed));

struct multiboot1_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

struct multiboot1_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} __attribute__((packed));

#endif /* _BOOT_MULTIBOOT1_H */
