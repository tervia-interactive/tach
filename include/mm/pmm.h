/*
 * mm/pmm.h - Physical Memory Manager (page frames)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _MM_PMM_H
#define _MM_PMM_H

#include <kernel/types.h>
#include <hal/mm.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

void pmm_init(const mem_region_t* memory_map, size_t region_count);
phys_addr_t pmm_alloc_frame(void);
bool pmm_retain_frame(phys_addr_t frame);
void pmm_free_frame(phys_addr_t frame);
size_t pmm_frame_refcount(phys_addr_t frame);
void* pmm_alloc_page(void);
void pmm_free_page(void* page);
void* pmm_alloc_pages(size_t count);
void* pmm_alloc_aligned_pages(size_t count, size_t alignment_pages);
void pmm_free_pages(void* pages, size_t count);
size_t pmm_get_free_pages(void);
size_t pmm_get_total_pages(void);
void pmm_mark_region_used(phys_addr_t start, size_t size);
void pmm_mark_region_free(phys_addr_t start, size_t size);

#endif /* _MM_PMM_H */
