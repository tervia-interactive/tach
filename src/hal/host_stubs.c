/* tach - Host test stubs for architecture-specific functions */
#include <hal/smp.h>
#include <proc/process.h>

/*
 * These stubs only exist for host unit tests. The kernel build compiles every
 * .c file in src/hal, so without this guard they collide with the real
 * arch_* implementations in src/arch/<arch>/ at link time.
 */
#ifdef TACH_HOST_TEST

/* Stub implementations for host testing */
uint32_t arch_smp_current_cpu(void) {
    return 0; /* Always return CPU 0 for host tests */
}

int arch_smp_cpu_count(void) {
    return 1; /* Single CPU for host tests */
}

bool arch_user_supported(void) {
    return true; /* Support user mode for host tests */
}

#endif /* TACH_HOST_TEST */
