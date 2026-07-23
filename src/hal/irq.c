/* tach - HAL IRQ Implementation */
#include <kernel/types.h>
#include <hal/irq.h>

void hal_irq_enable(void) {}
void hal_irq_disable(void) {}
int hal_irq_register(int irq, irq_handler_t handler, void* arg) {
    (void)irq;
    (void)handler;
    (void)arg;
    return 0;
}
