/* tach Operating System - HAL CPU Header */

#ifndef _HAL_CPU_H
#define _HAL_CPU_H

#include "kernel/types.h"

void hal_cpu_halt(void);
void hal_cpu_cli(void);
void hal_cpu_sti(void);
void hal_cpu_idle(void);
uint32_t hal_cpu_get_id(void);

#endif /* _HAL_CPU_H */
