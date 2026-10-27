/* tach - Host test stubs for architecture-specific functions */
#include <hal/smp.h>
#include <proc/process.h>

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