#include <kernel/types.h>
#include <hal/irq.h>

extern uint32_t gic_acknowledge(void);
extern void gic_end_interrupt(uint32_t token);

void arm_irq_handler(void) {
    uint32_t token = gic_acknowledge();
    uint32_t irq = token & 0x3ffu;
    if (irq < 1020u) {
        gic_end_interrupt(token);
        hal_irq_dispatch((int)irq);
    }
}
