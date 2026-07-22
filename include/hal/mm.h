/* tach Operating System - HAL MM Header */

#ifndef _HAL_MM_H
#define _HAL_MM_H

#include "kernel/types.h"

typedef struct {
    uint64_t base;
    uint64_t size;
    uint32_t type;
} mem_region_t;

#define MEM_REGION_USABLE     1
#define MEM_REGION_RESERVED   2
#define MEM_REGION_ACPI       3
#define MEM_REGION_NVS        4

void hal_mmu_init(void);
int hal_mmu_map(uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags);
int hal_mmu_unmap(uintptr_t virt, size_t size);
int hal_get_memmap(mem_region_t* regions, int max_regions);

#endif /* _HAL_MM_H */
