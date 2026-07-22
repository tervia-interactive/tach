/*
 * mm/slab.h - Slab allocator for small fixed-size objects
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _MM_SLAB_H
#define _MM_SLAB_H

#include <kernel/types.h>

struct slab_cache {
    const char* name;
    size_t object_size;
    size_t objects_per_slab;
    void* slabs;
};

void slab_init(void);
struct slab_cache* slab_cache_create(const char* name, size_t size);
void slab_cache_destroy(struct slab_cache* cache);
void* slab_alloc(struct slab_cache* cache);
void slab_free(struct slab_cache* cache, void* obj);

#endif /* _MM_SLAB_H */
