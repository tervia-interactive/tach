/* tach Operating System - Reference Counting Header */
/* Kernel-side reference counting helper (used by VFS, proc) */

#ifndef _KERNEL_REFCOUNT_H
#define _KERNEL_REFCOUNT_H

#include "kernel/types.h"
#include "kernel/atomic.h"

/* Reference count structure */
typedef struct refcount {
    atomic_int count;
} refcount_t;

/* Static initializer */
#define REFCOUNT_INIT { .count = 1 }

/* Initialize refcount */
void refcount_init(refcount_t* rc);

/* Increment reference count */
void refcount_inc(refcount_t* rc);

/* Decrement reference count, returns true if count reached zero */
bool refcount_dec(refcount_t* rc);

/* Get current count */
int refcount_get(refcount_t* rc);

/* Convenience macro for releasing with callback */
#define REFCOUNT_RELEASE(rc, free_fn) do { \
    if (refcount_dec(rc)) { \
        free_fn(rc); \
    } \
} while(0)

#endif /* _KERNEL_REFCOUNT_H */
