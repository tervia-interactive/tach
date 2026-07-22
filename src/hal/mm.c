/* tach Operating System - HAL MM Implementation */
#include <kernel/types.h>
#include <kernel/compiler.h>
#include <hal/mm.h>

/* These are generic no-op fallbacks. Some architectures (riscv32/riscv64)
 * provide their own real implementation of these same functions; mark these
 * weak so the arch-specific one wins the link instead of causing a
 * multiple-definition error. */
WEAK void hal_mmu_init(void) {}

WEAK int hal_mmu_map(uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags) {
    (void)virt;
    (void)phys;
    (void)size;
    (void)flags;
    return 0;
}

WEAK int hal_mmu_unmap(uintptr_t virt, size_t size) {
    (void)virt;
    (void)size;
    return 0;
}

int hal_get_memmap(mem_region_t* regions, int max_regions) {
    (void)regions;
    (void)max_regions;
    return 0;
}
