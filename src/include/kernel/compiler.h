/* tach Operating System - Compiler Macros Header */
/* Build macros for compiler-specific features */

#ifndef _KERNEL_COMPILER_H
#define _KERNEL_COMPILER_H

/* Section attributes */
#define SECTION(x) __attribute__((section(x)))
#define MULTIBOOT_SECTION SECTION(".multiboot")

/* Alignment */
#define ALIGN(x) __attribute__((aligned(x)))
#define PACKED __attribute__((packed))

/* Function attributes */
#define NORETURN __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#define USED __attribute__((used))
#define WEAK __attribute__((weak))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))

/* Branch prediction hints */
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* Format checking */
#define PRINTF_FORMAT(fmt_idx, first_arg) \
    __attribute__((format(printf, fmt_idx, first_arg)))

/* Static assertion (C11 style) */
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

/* Barrier macros */
#define COMPILER_BARRIER() asm volatile("" ::: "memory")

/* CPU relax/pause hint used while spinning on a lock. Each architecture has
 * its own instruction for this, so pick the right one at compile time
 * instead of hardcoding the x86 'pause' instruction (which is not a valid
 * opcode on ARM/AArch64/RISC-V and breaks the build there). */
#if defined(__x86_64__) || defined(__i386__)
#define CPU_PAUSE() asm volatile("pause" ::: "memory")
#elif defined(__aarch64__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#elif defined(__arm__)
#define CPU_PAUSE() asm volatile("yield" ::: "memory")
#elif defined(__riscv)
#define CPU_PAUSE() asm volatile("nop" ::: "memory")
#else
#define CPU_PAUSE() COMPILER_BARRIER()
#endif

/* Optimization barriers */
#define BARRIER() __asm__ __volatile__("" ::: "memory")

#endif /* _KERNEL_COMPILER_H */
