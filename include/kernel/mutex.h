/* tach Operating System - Mutex Header */
/* Sleeping mutex for kernel code paths that may block */

#ifndef _KERNEL_MUTEX_H
#define _KERNEL_MUTEX_H

#include "kernel/types.h"
#include "kernel/atomic.h"

/* Mutex structure */
typedef struct mutex {
    atomic_int state;
    void* owner;      /* Owning thread/process */
    void* waiters;    /* Wait queue */
} mutex_t;

/* Static initializer */
#define MUTEX_INIT { .state = 0, .owner = NULL, .waiters = NULL }

/* Initialize mutex */
void mutex_init(mutex_t* mtx);

/* Lock mutex (blocking) */
void mutex_lock(mutex_t* mtx);

/* Unlock mutex */
void mutex_unlock(mutex_t* mtx);

/* Try to lock mutex (non-blocking) */
bool mutex_trylock(mutex_t* mtx);

/* Destroy mutex */
void mutex_destroy(mutex_t* mtx);

#endif /* _KERNEL_MUTEX_H */
