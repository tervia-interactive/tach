/*
 * mm/pmm.h - Physical Memory Manager (page frames)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _MM_PMM_H
#define _MM_PMM_H

#include <kernel/types.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

void pmm_init(void* memory_map, size_t map_size);
void* pmm_alloc_page(void);
void pmm_free_page(void* page);
void* pmm_alloc_pages(size_t count);
void pmm_free_pages(void* pages, size_t count);
size_t pmm_get_free_pages(void);
void pmm_mark_region_used(phys_addr_t start, size_t size);
void pmm_mark_region_free(phys_addr_t start, size_t size);

#endif /* _MM_PMM_H */
