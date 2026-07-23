/* tach - Spinlock Header */
/* Spinlocks for short critical sections (IRQ-safe variants) */

#ifndef _KERNEL_SPINLOCK_H
#define _KERNEL_SPINLOCK_H

#include "kernel/types.h"
#include "kernel/atomic.h"

/* Spinlock structure */
typedef struct {
    atomic_int locked;
    uint32_t cpu_id;
} spinlock_t;

/* Static initializer */
#define SPINLOCK_INIT { .locked = 0, .cpu_id = 0 }

/* Initialize spinlock */
void spinlock_init(spinlock_t* lock);

/* Acquire spinlock */
void spinlock_lock(spinlock_t* lock);

/* Release spinlock */
void spinlock_unlock(spinlock_t* lock);

/* Try to acquire spinlock (non-blocking) */
bool spinlock_trylock(spinlock_t* lock);

/* IRQ-safe spinlock acquire (disables interrupts) */
void spinlock_lock_irq(spinlock_t* lock);

/* IRQ-safe spinlock release (restores interrupts) */
void spinlock_unlock_irq(spinlock_t* lock);

#endif /* _KERNEL_SPINLOCK_H */
