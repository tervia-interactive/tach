/*
 * mm/kheap.h - Kernel heap (kmalloc, kfree)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _MM_KHEAP_H
#define _MM_KHEAP_H

#include <kernel/types.h>

void kheap_init(void* start, size_t size);
void* kmalloc(size_t size);
void* kzalloc(size_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t new_size);

#endif /* _MM_KHEAP_H */
