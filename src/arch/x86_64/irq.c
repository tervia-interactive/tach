#include <kernel/types.h>
#include <hal/irq.h>

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void irq_install(void) {
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask = inb(0xa1);
    outb(0x20, 0x11);
    outb(0xa0, 0x11);
    outb(0x21, 0x20);
    outb(0xa1, 0x28);
    outb(0x21, 0x04);
    outb(0xa1, 0x02);
    outb(0x21, 0x01);
    outb(0xa1, 0x01);
    outb(0x21, (uint8_t)(master_mask & ~0x03u));
    outb(0xa1, slave_mask);
}

void x86_irq_handler(uint64_t irq) {
    if (irq >= 8) outb(0xa0, 0x20);
    outb(0x20, 0x20);
    hal_irq_dispatch((int)irq);
}

void irq_enable(void) { hal_irq_enable(); }
void irq_disable(void) { hal_irq_disable(); }
