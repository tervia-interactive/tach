/* tach Operating System - Spinlock Implementation */
#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <kernel/atomic.h>

void spinlock_init(spinlock_t* lock) {
    atomic_store(&lock->locked, 0);
    lock->cpu_id = 0;
}

void spinlock_lock(spinlock_t* lock) {
    while (!atomic_compare_exchange_weak(&lock->locked, 0, 1)) {
        /* Spin until lock is acquired */
        __asm__ volatile("pause" ::: "memory");
    }
}

void spinlock_unlock(spinlock_t* lock) {
    atomic_store(&lock->locked, 0);
}

bool spinlock_trylock(spinlock_t* lock) {
    return atomic_compare_exchange_weak(&lock->locked, 0, 1);
}

void spinlock_lock_irq(spinlock_t* lock) {
    __asm__ volatile("cli" ::: "memory");
    spinlock_lock(lock);
}

void spinlock_unlock_irq(spinlock_t* lock) {
    spinlock_unlock(lock);
    __asm__ volatile("sti" ::: "memory");
}
