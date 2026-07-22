#include <hal/irq.h>
void hal_irq_enable(void) {}
void hal_irq_disable(void) {}
int hal_irq_register(int irq, void (*handler)(void)) {(void)irq;(void)handler; return 0;}
