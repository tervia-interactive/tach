/* tach Operating System - Semaphore Implementation */
#include <kernel/types.h>
#include <kernel/sem.h>

/* Simple counting semaphore implementation */
static sem_handle_t next_handle = 1;
static int sem_values[256];
static bool sem_initialized[256];

sem_handle_t sem_create(int initial_value) {
    if (next_handle >= 256) {
        return 0; /* Error: no more handles */
    }
    sem_handle_t handle = next_handle++;
    sem_values[handle] = initial_value;
    sem_initialized[handle] = true;
    return handle;
}

int sem_acquire(sem_handle_t sem, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (sem == 0 || sem >= 256 || !sem_initialized[sem]) {
        return -1;
    }
    while (sem_values[sem] <= 0) {
        /* In real implementation, would block here */
        __asm__ volatile("hlt");
    }
    sem_values[sem]--;
    return 0;
}

int sem_release(sem_handle_t sem) {
    if (sem == 0 || sem >= 256 || !sem_initialized[sem]) {
        return -1;
    }
    sem_values[sem]++;
    return 0;
}

int sem_tryacquire(sem_handle_t sem) {
    if (sem == 0 || sem >= 256 || !sem_initialized[sem]) {
        return -1;
    }
    if (sem_values[sem] <= 0) {
        return -1;
    }
    sem_values[sem]--;
    return 0;
}

int sem_get_value(sem_handle_t sem) {
    if (sem == 0 || sem >= 256 || !sem_initialized[sem]) {
        return -1;
    }
    return sem_values[sem];
}

void sem_destroy(sem_handle_t sem) {
    if (sem == 0 || sem >= 256) {
        return;
    }
    sem_initialized[sem] = false;
}
