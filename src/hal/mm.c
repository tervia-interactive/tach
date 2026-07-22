/* tach Operating System - HAL MM Implementation */
#include <kernel/types.h>
#include <hal/hal_mm.h>

void hal_mmu_init(void) {}

int hal_mmu_map(uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags) {
    (void)virt;
    (void)phys;
    (void)size;
    (void)flags;
    return 0;
}

int hal_mmu_unmap(uintptr_t virt, size_t size) {
    (void)virt;
    (void)size;
    return 0;
}

int hal_get_memmap(mem_region_t* regions, int max_regions) {
    (void)regions;
    (void)max_regions;
    return 0;
}
