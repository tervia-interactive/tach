/* RISC-V 64-bit PLIC (Platform-Level Interrupt Controller) */

#include <hal/irq.h>
#include <kernel/types.h>

#define PLIC_BASE 0x0C000000UL
#define PLIC_PRIORITY(i) (PLIC_BASE + (i) * 4)
#define PLIC_PENDING(i) (PLIC_BASE + 0x1000 + ((i) / 32) * 4)
#define PLIC_ENABLE(i) (PLIC_BASE + 0x2000 + ((i) / 32) * 4)
#define PLIC_THRESHOLD 0x200000UL
#define PLIC_CLAIM 0x200004UL

static int plic_initialized = 0;

int plic_init(void) {
    plic_initialized = 1;
    return 0;
}

int plic_irq_enable(int irq) {
    if (!plic_initialized) return -1;
    
    uint32_t *enable = (uint32_t *)PLIC_ENABLE(irq);
    *enable |= (1 << (irq % 32));
    
    return 0;
}

int plic_irq_disable(int irq) {
    if (!plic_initialized) return -1;
    
    uint32_t *enable = (uint32_t *)PLIC_ENABLE(irq);
    *enable &= ~(1 << (irq % 32));
    
    return 0;
}

int plic_irq_ack(int irq) {
    if (!plic_initialized) return -1;
    
    volatile uint32_t *claim = (uint32_t *)PLIC_CLAIM;
    (void)*claim;
    
    return 0;
}
