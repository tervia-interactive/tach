/* tach - Blocking FIFO semaphore */
#include <ipc/sem.h>
#include <kernel/assert.h>
#include <kernel/string.h>
#include <proc/scheduler.h>

void sem_init(sem_t* semaphore, int32_t value) {
    KASSERT(semaphore != NULL);
    KASSERT(value >= 0);
    semaphore->value = value;
    semaphore->waiters = 0;
    semaphore->wait_head = 0;
    semaphore->wait_tail = 0;
    semaphore->wait_count = 0;
    memset(semaphore->wait_queue, 0, sizeof(semaphore->wait_queue));
    spinlock_init(&semaphore->lock);
}

void sem_acquire(sem_t* semaphore) {
    KASSERT(semaphore != NULL);
    spinlock_lock(&semaphore->lock);
    if (semaphore->value > 0) {
        semaphore->value--;
        spinlock_unlock(&semaphore->lock);
        return;
    }

    struct process* current = process_get_current();
    KASSERT(current != NULL);
    KASSERT(semaphore->wait_count < MAX_PROCESSES);
    semaphore->wait_queue[semaphore->wait_tail] = current;
    semaphore->wait_tail++;
    if (semaphore->wait_tail >= MAX_PROCESSES) semaphore->wait_tail = 0;
    semaphore->wait_count++;
    semaphore->waiters = (int32_t)semaphore->wait_count;

    /*
     * Mark the process blocked before dropping the lock. A concurrent
     * release can then transfer the permit and wake it without the classic
     * lost-wakeup window between queue insertion and scheduler_yield().
     */
    current->block_reason = semaphore;
    current->state = PROCESS_STATE_BLOCKED;
    spinlock_unlock(&semaphore->lock);
    scheduler_yield();
}

void sem_release(sem_t* semaphore) {
    KASSERT(semaphore != NULL);
    spinlock_lock(&semaphore->lock);
    if (semaphore->wait_count) {
        struct process* process =
            semaphore->wait_queue[semaphore->wait_head];
        semaphore->wait_queue[semaphore->wait_head] = NULL;
        semaphore->wait_head++;
        if (semaphore->wait_head >= MAX_PROCESSES) semaphore->wait_head = 0;
        semaphore->wait_count--;
        semaphore->waiters = (int32_t)semaphore->wait_count;
        scheduler_wake(process);
    } else {
        semaphore->value++;
    }
    spinlock_unlock(&semaphore->lock);
}
