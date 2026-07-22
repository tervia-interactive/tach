#include <kernel/types.h>
void paging_init(void) { }
void page_map(uint64_t virt, uint64_t phys) { (void)virt; (void)phys; }
void page_unmap(uint64_t virt) { (void)virt; }
