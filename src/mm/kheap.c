#include <mm/kheap.h>
void *kmalloc(size_t s) {(void)s; return (void*)0;}
void kfree(void *p) {(void)p;}
