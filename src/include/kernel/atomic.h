/* tach - Atomic Operations Header */
/* Atomic ops (CAS, fetch-add, load/store-acquire) — SMP safety */

#ifndef _KERNEL_ATOMIC_H
#define _KERNEL_ATOMIC_H

#include "kernel/types.h"
#include "kernel/compiler.h"

/* Atomic integer type */
typedef volatile int32_t atomic_int;
typedef volatile uint32_t atomic_uint;
typedef volatile int64_t atomic_int64;
typedef volatile uint64_t atomic_uint64;
typedef volatile bool atomic_bool;

/* Memory ordering */
#define MEMORY_ORDER_RELAXED  0
#define MEMORY_ORDER_ACQUIRE  1
#define MEMORY_ORDER_RELEASE  2
#define MEMORY_ORDER_SEQ_CST  3

/* Compare-and-swap (CAS) */
static ALWAYS_INLINE bool atomic_cas(atomic_int* ptr, int32_t expected, int32_t desired) {
#if defined(__x86_64__) || defined(__i386__)
    int32_t prev;
    asm volatile("lock cmpxchg %3, %0"
                 : "+m" (*ptr), "=a" (prev)
                 : "a" (expected), "r" (desired)
                 : "memory", "cc");
    return prev == expected;
#else
    /* Fallback - architecture specific implementation needed */
    return __sync_bool_compare_and_swap(ptr, expected, desired);
#endif
}

/* Atomic load (simple) */
static ALWAYS_INLINE int32_t atomic_load(const atomic_int* ptr) {
    return *ptr;
}

/* Atomic store (simple) */
static ALWAYS_INLINE void atomic_store(atomic_int* ptr, int32_t val) {
    *ptr = val;
}

/* Compare-and-exchange weak */
static ALWAYS_INLINE bool atomic_compare_exchange_weak(atomic_int* ptr, int32_t expected, int32_t desired) {
    return atomic_cas(ptr, expected, desired);
}

/* Atomic load with acquire semantics */
static ALWAYS_INLINE int32_t atomic_load_acquire(const atomic_int* ptr) {
    int32_t val = *ptr;
    COMPILER_BARRIER();
#if defined(__x86_64__) || defined(__i386__)
    asm volatile("lfence" ::: "memory");
#endif
    return val;
}

/* Atomic store with release semantics */
static ALWAYS_INLINE void atomic_store_release(atomic_int* ptr, int32_t val) {
    COMPILER_BARRIER();
#if defined(__x86_64__) || defined(__i386__)
    asm volatile("sfence" ::: "memory");
#endif
    *ptr = val;
}

/* Fetch-and-add */
static ALWAYS_INLINE int32_t atomic_fetch_add(atomic_int* ptr, int32_t val) {
#if defined(__x86_64__) || defined(__i386__)
    asm volatile("lock xadd %0, %1"
                 : "+r" (val), "+m" (*ptr)
                 : : "memory", "cc");
    return val;
#else
    return __sync_fetch_and_add(ptr, val);
#endif
}

/* Fetch-and-sub */
static ALWAYS_INLINE int32_t atomic_fetch_sub(atomic_int* ptr, int32_t val) {
    return atomic_fetch_add(ptr, -val);
}

/* Increment atomically */
static ALWAYS_INLINE void atomic_inc(atomic_int* ptr) {
    atomic_fetch_add(ptr, 1);
}

/* Decrement atomically */
static ALWAYS_INLINE void atomic_dec(atomic_int* ptr) {
    atomic_fetch_sub(ptr, 1);
}

#endif /* _KERNEL_ATOMIC_H */
