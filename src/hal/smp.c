#include <kernel/types.h>
#include <hal/smp.h>
int hal_smp_boot_secondary(int cpu) {(void)cpu; return 0;}
int hal_smp_cpu_count(void) { return 1; }
