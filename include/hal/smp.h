/* tach - HAL SMP Header */

#ifndef _HAL_SMP_H
#define _HAL_SMP_H

#include "kernel/types.h"

int hal_smp_boot_secondary(uint32_t cpu_id, void* entry);
int hal_smp_cpu_count(void);
void hal_smp_send_ipi(uint32_t cpu_id, uint32_t vector);
void hal_smp_init(void);

#endif /* _HAL_SMP_H */
