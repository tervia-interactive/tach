/* tach Operating System - Spinlock Implementation */
#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <kernel/atomic.h>
#include <kernel/compiler.h>
#include <hal/cpu.h>

void spinlock_init(spinlock_t* lock) {
    atomic_store(&lock->locked, 0);
    lock->cpu_id = 0;
}

void spinlock_lock(spinlock_t* lock) {
    while (!atomic_compare_exchange_weak(&lock->locked, 0, 1)) {
        /* Spin until lock is acquired */
        CPU_PAUSE();
    }
}

void spinlock_unlock(spinlock_t* lock) {
    atomic_store(&lock->locked, 0);
}

bool spinlock_trylock(spinlock_t* lock) {
    return atomic_compare_exchange_weak(&lock->locked, 0, 1);
}

void spinlock_lock_irq(spinlock_t* lock) {
    hal_cpu_cli();
    spinlock_lock(lock);
}

void spinlock_unlock_irq(spinlock_t* lock) {
    spinlock_unlock(lock);
    hal_cpu_sti();
}
