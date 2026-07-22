#include <kernel/types.h>
#include <hal/hal_mm.h>
void hal_mmu_init(void) {}
int hal_mmu_map(uintptr_t v, uintptr_t p, size_t s) {(void)v;(void)p;(void)s; return 0;}
int hal_get_memmap(void *map, size_t max) {(void)map;(void)max; return 0;}
