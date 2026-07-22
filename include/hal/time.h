/* tach Operating System - HAL Time Header */

#ifndef _HAL_TIME_H
#define _HAL_TIME_H

#include "kernel/types.h"

void hal_timer_init(void);
uint64_t hal_timer_get_ticks(void);
uint64_t hal_timer_get_frequency(void);
void hal_timer_sleep(uint64_t ms);

#endif /* _HAL_TIME_H */
