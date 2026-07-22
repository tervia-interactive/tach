#include <kernel/types.h>
void mmu_init(void) { }
void mmu_map(uint32_t virt, uint32_t phys) { (void)virt; (void)phys; }
void mmu_unmap(uint32_t virt) { (void)virt; }
