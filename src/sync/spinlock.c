/* tach - Spinlock Implementation */
#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <kernel/atomic.h>
#include <kernel/compiler.h>
#include <hal/cpu.h>
#include <hal/irq.h>

void spinlock_init(spinlock_t* lock) {
    atomic_store(&lock->locked, 0);
    lock->cpu_id = 0;
    lock->irq_flags = 0;
}

void spinlock_lock(spinlock_t* lock) {
    irq_flags_t flags = hal_irq_save();
    while (!atomic_compare_exchange_weak(&lock->locked, 0, 1)) {
        /* Spin until lock is acquired */
        CPU_PAUSE();
    }
    lock->irq_flags = flags;
}

void spinlock_unlock(spinlock_t* lock) {
    irq_flags_t flags = lock->irq_flags;
    atomic_store(&lock->locked, 0);
    hal_irq_restore(flags);
}

bool spinlock_trylock(spinlock_t* lock) {
    irq_flags_t flags = hal_irq_save();
    if (!atomic_compare_exchange_weak(&lock->locked, 0, 1)) {
        hal_irq_restore(flags);
        return false;
    }
    lock->irq_flags = flags;
    return true;
}

void spinlock_lock_irq(spinlock_t* lock) {
    spinlock_lock(lock);
}

void spinlock_unlock_irq(spinlock_t* lock) {
    spinlock_unlock(lock);
}
