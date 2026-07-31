/* tach - HAL IRQ Header */

#ifndef _HAL_IRQ_H
#define _HAL_IRQ_H

#include "kernel/types.h"

typedef void (*irq_handler_t)(void* arg);
typedef uintptr_t irq_flags_t;

void hal_irq_enable(void);
void hal_irq_disable(void);
bool hal_irq_is_enabled(void);
irq_flags_t hal_irq_save(void);
void hal_irq_restore(irq_flags_t flags);
int hal_irq_register(int irq, irq_handler_t handler, void* arg);
int hal_irq_unregister(int irq);
void hal_irq_dispatch(int irq);

#endif /* _HAL_IRQ_H */
