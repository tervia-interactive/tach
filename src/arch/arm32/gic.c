#include <kernel/types.h>

#define GICD_BASE 0x08000000u
#define GICC_BASE 0x08010000u

static inline void write32(uintptr_t address, uint32_t value) {
    *(volatile uint32_t*)address = value;
}
static inline uint32_t read32(uintptr_t address) {
    return *(volatile uint32_t*)address;
}

void gic_init(void) {
    write32(GICD_BASE, 0);
    write32(GICC_BASE + 4, 0xff);
    write32(GICC_BASE, 1);
    write32(GICD_BASE, 1);
}
void gic_enable_irq(uint32_t irq) {
    write32(GICD_BASE + 0x400 + (irq & ~3u), 0x80808080u);
    write32(GICD_BASE + 0x100 + (irq / 32u) * 4u,
            1u << (irq % 32u));
}
void gic_disable_irq(uint32_t irq) {
    write32(GICD_BASE + 0x180 + (irq / 32u) * 4u,
            1u << (irq % 32u));
}
uint32_t gic_acknowledge(void) { return read32(GICC_BASE + 0x0c); }
void gic_end_interrupt(uint32_t token) { write32(GICC_BASE + 0x10, token); }
