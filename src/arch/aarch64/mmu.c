#include <kernel/types.h>
void mmu_init(void) { }
void mmu_map(uint64_t virt, uint64_t phys) { (void)virt; (void)phys; }
void mmu_unmap(uint64_t virt) { (void)virt; }
