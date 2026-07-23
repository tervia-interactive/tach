/* tach - Semaphore Header */
/* Counting semaphores */

#ifndef _KERNEL_SEM_H
#define _KERNEL_SEM_H

#include "kernel/types.h"

/* Semaphore handle */
typedef uint32_t sem_handle_t;

/* Create semaphore */
sem_handle_t sem_create(int initial_value);

/* Acquire semaphore (blocking) */
int sem_acquire(sem_handle_t sem, uint32_t timeout_ms);

/* Release semaphore */
int sem_release(sem_handle_t sem);

/* Try to acquire (non-blocking) */
int sem_tryacquire(sem_handle_t sem);

/* Get semaphore value */
int sem_get_value(sem_handle_t sem);

/* Destroy semaphore */
void sem_destroy(sem_handle_t sem);

#endif /* _KERNEL_SEM_H */
