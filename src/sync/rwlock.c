/* tach - Reader/Writer Lock Implementation */
#include <kernel/types.h>
#include <kernel/rwlock.h>
#include <kernel/atomic.h>
#include <kernel/compiler.h>

void rwlock_init(rwlock_t* lock) {
    atomic_store(&lock->readers, 0);
    atomic_store(&lock->writers, 0);
    lock->write_waiters = NULL;
    lock->read_waiters = NULL;
}

void rwlock_read_lock(rwlock_t* lock) {
    /* Simple implementation: wait for no writers */
    while (atomic_load(&lock->writers) != 0) {
        CPU_PAUSE();
    }
    atomic_fetch_add(&lock->readers, 1);
}

void rwlock_read_unlock(rwlock_t* lock) {
    atomic_fetch_sub(&lock->readers, 1);
}

void rwlock_write_lock(rwlock_t* lock) {
    /* Wait for no readers and no writers */
    while (atomic_load(&lock->readers) != 0 || atomic_load(&lock->writers) != 0) {
        CPU_PAUSE();
    }
    atomic_store(&lock->writers, 1);
}

void rwlock_write_unlock(rwlock_t* lock) {
    atomic_store(&lock->writers, 0);
}

void rwlock_destroy(rwlock_t* lock) {
    (void)lock;
}
