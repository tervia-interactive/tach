/*
 * hw/fdt.h - Flattened Device Tree parser (libfdt wrapper)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _HW_FDT_H
#define _HW_FDT_H

#include <kernel/types.h>

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} __attribute__((packed));

#define FDT_MAGIC 0xd00dfeed

void fdt_init(void* fdt_blob);
int fdt_get_node_offset(const char* path);
const void* fdt_getprop(int node, const char* name, int* len);
int fdt_next_node(int node);
uint64_t fdt_get_address(int node);
uint64_t fdt_get_size(int node);

#endif /* _HW_FDT_H */
