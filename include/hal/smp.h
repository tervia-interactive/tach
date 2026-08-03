/* tach - HAL SMP Header */

#ifndef _HAL_SMP_H
#define _HAL_SMP_H

#include "kernel/types.h"

int hal_smp_boot_secondary(uint32_t cpu_id, void* entry);
int hal_smp_cpu_count(void);
uint32_t hal_smp_current_cpu(void);
bool hal_smp_cpu_online(uint32_t cpu_id);
void hal_smp_send_ipi(uint32_t cpu_id, uint32_t vector);
void hal_smp_init(void);
void hal_smp_secondary_entry(uint32_t cpu_id) __attribute__((noreturn));

#endif /* _HAL_SMP_H */
