#include <kernel/types.h>
#include <mm/pmm.h>
void pmm_init(void) { }
void *pmm_alloc_frame(void) { return (void*)0; }
void pmm_free_frame(void *f) {(void)f;}
