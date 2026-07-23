/* tach - Virtual Memory Manager Implementation */
#include <kernel/types.h>
#include <kernel/assert.h>
#include <mm/vmm.h>

void vmm_init(void) {
}

int vmm_map(struct vmm_context* ctx, void* virt, phys_addr_t phys, uint32_t flags) {
    KASSERT(ctx != NULL);
    KASSERT(virt != NULL);
    (void)phys;
    (void)flags;
    return 0;
}

int vmm_unmap(struct vmm_context* ctx, void* virt) {
    KASSERT(ctx != NULL);
    KASSERT(virt != NULL);
    return 0;
}

void* vmm_alloc_page(struct vmm_context* ctx, uint32_t flags) {
    KASSERT(ctx != NULL);
    (void)flags;
    return (void*)0;
}

void vmm_free_page(struct vmm_context* ctx, void* virt) {
    KASSERT(ctx != NULL);
    (void)virt;
}
