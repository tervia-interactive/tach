#include <mm/pmm.h>
int pmm_init(void) { return 0; }
void *pmm_alloc_frame(void) { return (void*)0; }
void pmm_free_frame(void *f) {(void)f;}
