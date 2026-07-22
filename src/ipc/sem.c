/* tach Operating System - Semaphore Implementation */
#include <ipc/sem.h>
#include <kernel/assert.h>

void sem_init(sem_t *s, int32_t value) {
    KASSERT(s != NULL);
    s->value = value;
    s->waiters = 0;
}

void sem_acquire(sem_t *s) {
    KASSERT(s != NULL);
    /* TODO: Implement proper blocking when value is 0 */
    while (s->value <= 0) {
        s->waiters++;
        /* TODO: Block current thread */
        s->waiters--;
    }
    s->value--;
}

void sem_release(sem_t *s) {
    KASSERT(s != NULL);
    s->value++;
    /* TODO: Wake up a waiter if any */
}
