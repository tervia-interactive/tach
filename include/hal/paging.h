/*
 * hal/paging.h - Architecture page-table interface
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _HAL_PAGING_H
#define _HAL_PAGING_H

#include <kernel/types.h>

int arch_paging_init(void);
void* arch_paging_kernel_root(void);
void* arch_paging_create_root(void);
void arch_paging_destroy_root(void* root);
void arch_paging_switch(void* root);
int arch_paging_map(void* root, uintptr_t virt, phys_addr_t phys,
                    uint32_t flags);
int arch_paging_unmap(void* root, uintptr_t virt);

#endif /* _HAL_PAGING_H */
