/* tach Operating System - HAL IRQ Header */

#ifndef _HAL_IRQ_H
#define _HAL_IRQ_H

#include "kernel/types.h"

typedef void (*irq_handler_t)(void* arg);

void hal_irq_enable(void);
void hal_irq_disable(void);
bool hal_irq_is_enabled(void);
int hal_irq_register(int irq, irq_handler_t handler, void* arg);
int hal_irq_unregister(int irq);

#endif /* _HAL_IRQ_H */
