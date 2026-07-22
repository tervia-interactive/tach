/* tach Operating System - Reader/Writer Lock Header */

#ifndef _KERNEL_RWLOCK_H
#define _KERNEL_RWLOCK_H

#include "kernel/types.h"
#include "kernel/atomic.h"

/* RWLock structure */
typedef struct rwlock {
    atomic_int readers;
    atomic_int writers;
    void* write_waiters;
    void* read_waiters;
} rwlock_t;

/* Static initializer */
#define RWLOCK_INIT { .readers = 0, .writers = 0, .write_waiters = NULL, .read_waiters = NULL }

/* Initialize rwlock */
void rwlock_init(rwlock_t* lock);

/* Acquire read lock */
void rwlock_read_lock(rwlock_t* lock);

/* Release read lock */
void rwlock_read_unlock(rwlock_t* lock);

/* Acquire write lock */
void rwlock_write_lock(rwlock_t* lock);

/* Release write lock */
void rwlock_write_unlock(rwlock_t* lock);

/* Destroy rwlock */
void rwlock_destroy(rwlock_t* lock);

#endif /* _KERNEL_RWLOCK_H */
