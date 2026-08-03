#include <kernel/types.h>

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL
#define GICD_CTLR 0x000
#define GICD_ISENABLER 0x100
#define GICD_IPRIORITYR 0x400
#define GICC_CTLR 0x000
#define GICC_PMR 0x004
#define GICC_IAR 0x00c
#define GICC_EOIR 0x010

static inline void mmio_write(uintptr_t address, uint32_t value) {
    *(volatile uint32_t*)address = value;
}
static inline uint32_t mmio_read(uintptr_t address) {
    return *(volatile uint32_t*)address;
}

void gic_init(void) {
    mmio_write(GICD_BASE + GICD_CTLR, 0);
    mmio_write(GICC_BASE + GICC_PMR, 0xff);
    mmio_write(GICC_BASE + GICC_CTLR, 1);
    mmio_write(GICD_BASE + GICD_CTLR, 1);
}

void gic_enable_irq(uint32_t irq) {
    mmio_write(GICD_BASE + GICD_IPRIORITYR + (irq & ~3u), 0x80808080u);
    mmio_write(GICD_BASE + GICD_ISENABLER + (irq / 32u) * 4u,
               1u << (irq % 32u));
}

void gic_disable_irq(uint32_t irq) {
    mmio_write(GICD_BASE + 0x180 + (irq / 32u) * 4u,
               1u << (irq % 32u));
}

uint32_t gic_acknowledge(void) {
    return mmio_read(GICC_BASE + GICC_IAR);
}

void gic_end_interrupt(uint32_t token) {
    mmio_write(GICC_BASE + GICC_EOIR, token);
}
