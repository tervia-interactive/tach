/* tach - Semaphore Header */
#ifndef _IPC_SEM_H
#define _IPC_SEM_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <proc/process.h>
#include <stdint.h>

/* Semaphore structure */
typedef struct {
    int32_t value;
    int32_t waiters;
    struct process* wait_queue[MAX_PROCESSES];
    size_t wait_head;
    size_t wait_tail;
    size_t wait_count;
    spinlock_t lock;
} sem_t;

/* Initialize a semaphore with initial value */
void sem_init(sem_t *s, int32_t value);

/* Acquire semaphore (decrement, block if zero) */
void sem_acquire(sem_t *s);

/* Release semaphore (increment, wake waiter if any) */
void sem_release(sem_t *s);

#endif /* _IPC_SEM_H */
