#include <mm/vmm.h>
int vmm_init(void) { return 0; }
int vmm_map(uintptr_t v, uintptr_t p) {(void)v;(void)p; return 0;}
