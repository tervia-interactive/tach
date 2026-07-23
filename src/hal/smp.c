/* tach - HAL SMP Implementation */
#include <kernel/types.h>
#include <hal/smp.h>

int hal_smp_boot_secondary(uint32_t cpu_id, void* entry) {
    (void)cpu_id;
    (void)entry;
    return 0;
}

int hal_smp_cpu_count(void) {
    return 1;
}
