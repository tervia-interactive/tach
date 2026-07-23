/* tach - Assert Header */
/* KASSERT()/KASSERT_MSG() — panics on invariant violation */

#ifndef _KERNEL_ASSERT_H
#define _KERNEL_ASSERT_H

#include "kernel/compiler.h"

/* Forward declaration to avoid circular dependency */
struct kernel_panic_info;

/* Kernel assert that triggers panic */
#ifdef NDEBUG
    #define KASSERT(cond) ((void)0)
    #define KASSERT_MSG(cond, msg) ((void)0)
#else
    /* External function declaration */
    void kernel_panic_with_code(const char* reason, int code);
    
    #define KASSERT(cond) do { \
        if (unlikely(!(cond))) { \
            kernel_panic_with_code("Assertion failed: " #cond, __LINE__); \
        } \
    } while(0)

    #define KASSERT_MSG(cond, msg) do { \
        if (unlikely(!(cond))) { \
            kernel_panic_with_code("Assertion failed: " msg, __LINE__); \
        } \
    } while(0)
#endif

/* Compile-time assertion */
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

#endif /* _KERNEL_ASSERT_H */
