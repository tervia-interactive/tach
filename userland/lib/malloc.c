/**
 * @file malloc.c
 * @brief Simple memory allocator for tach userland
 * 
 * This is a basic bump allocator for early development.
 * Will be replaced with a proper slab/heap allocator.
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include <stddef.h>
#include <stdint.h>

#define HEAP_SIZE (1024 * 1024)  /* 1 MB heap */
#define ALIGNMENT 8

static uint8_t heap[HEAP_SIZE] __attribute__((aligned(ALIGNMENT)));
static size_t heap_top = 0;

static size_t align_up(size_t size) {
    return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

void *malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    size = align_up(size);
    
    if (heap_top + size > HEAP_SIZE) {
        return NULL;  /* Out of memory */
    }
    
    void *ptr = &heap[heap_top];
    heap_top += size;
    
    return ptr;
}

void free(void *ptr) {
    /* No-op in bump allocator - memory is never freed */
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void *ptr = malloc(total_size);
    
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < total_size; i++) {
            p[i] = 0;
        }
    }
    
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    /* Simple implementation: allocate new and copy */
    void *new_ptr = malloc(size);
    
    if (new_ptr && ptr) {
        /* We don't know the old size, so this is limited */
        uint8_t *src = (uint8_t *)ptr;
        uint8_t *dst = (uint8_t *)new_ptr;
        
        /* Copy up to the new size (may lose data if shrinking) */
        for (size_t i = 0; i < size; i++) {
            dst[i] = src[i];
        }
    }
    
    return new_ptr;
}
