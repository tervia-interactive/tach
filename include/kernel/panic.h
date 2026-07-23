/* tach - Kernel Panic Header */
/* Kernel panic functionality */

#ifndef _KERNEL_PANIC_H
#define _KERNEL_PANIC_H

#include "kernel/types.h"
#include "kernel/compiler.h"

/* Panic flags */
#define PANIC_FLAG_HALT     0x01
#define PANIC_FLAG_REBOOT   0x02
#define PANIC_FLAG_DUMP_REGS 0x04

/* Kernel panic function */
NORETURN void kernel_panic(const char* reason);
NORETURN void kernel_panic_with_code(const char* reason, int code);

/* Panic with register dump */
NORETURN void kernel_panic_dump(const char* reason, void* regs);

/* Set panic behavior */
void kernel_panic_set_flags(uint32_t flags);

/* Assert macro that panics on failure */
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

#endif /* _KERNEL_PANIC_H */
