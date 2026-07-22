/* RISC-V 32-bit SMP Support */

#include <hal/hal_smp.h>
#include <kernel/types.h>

static int cpu_count = 1;

int hal_smp_init(void) {
    cpu_count = 1;
    return 0;
}

int hal_smp_boot_secondary(int cpu_id, void *entry) {
    (void)cpu_id;
    (void)entry;
    return -1;
}

int hal_smp_cpu_count(void) {
    return cpu_count;
}
