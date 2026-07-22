/* tach Operating System - Mutex Implementation */
#include <kernel/types.h>
#include <kernel/mutex.h>
#include <kernel/atomic.h>

void mutex_init(mutex_t* mtx) {
    atomic_store(&mtx->state, 0);
    mtx->owner = NULL;
    mtx->waiters = NULL;
}

void mutex_lock(mutex_t* mtx) {
    /* Simple spinlock-based mutex (in real impl, would sleep) */
    while (!atomic_compare_exchange_weak(&mtx->state, 0, 1)) {
        __asm__ volatile("pause" ::: "memory");
    }
}

void mutex_unlock(mutex_t* mtx) {
    atomic_store(&mtx->state, 0);
}

bool mutex_trylock(mutex_t* mtx) {
    return atomic_compare_exchange_weak(&mtx->state, 0, 1);
}

void mutex_destroy(mutex_t* mtx) {
    (void)mtx;
}
