/*
 * mm/vmm.h - Virtual Memory Manager (page tables)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _MM_VMM_H
#define _MM_VMM_H

#include <kernel/types.h>

/* Forward declaration to avoid circular dependency with proc/process.h */
struct process;

#define VMM_PRESENT  (1 << 0)
#define VMM_WRITABLE (1 << 1)
#define VMM_USER     (1 << 2)
#define VMM_NOCACHE  (1 << 3)
#define VMM_EXECUTABLE (1 << 4)

struct vmm_context {
    void* page_table;
    pid_t owner;
    uintptr_t next_free;
};

void vmm_init(void);
struct vmm_context* vmm_create_context(void);
void vmm_destroy_context(struct vmm_context* ctx);
void vmm_switch_context(struct vmm_context* ctx);
int vmm_map(struct vmm_context* ctx, void* virt, phys_addr_t phys, uint32_t flags);
int vmm_map_allocated(struct vmm_context* ctx, void* virt, uint32_t flags,
                      phys_addr_t* phys_out);
int vmm_unmap(struct vmm_context* ctx, void* virt);
void* vmm_alloc_page(struct vmm_context* ctx, uint32_t flags);
void vmm_free_page(struct vmm_context* ctx, void* virt);
int vmm_handle_fault(uintptr_t addr, int is_write);
phys_addr_t vmm_resolve(struct vmm_context* ctx, const void* virt);
struct vmm_context* vmm_kernel_context(void);

#endif /* _MM_VMM_H */
