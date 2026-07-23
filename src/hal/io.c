/* tach - HAL I/O Implementation */
#include <kernel/types.h>
#include <hal/io.h>

#if defined(__i386__) || defined(__x86_64__)

uint8_t hal_port_in(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void hal_port_out(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint16_t hal_port_in16(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void hal_port_out16(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

#else

/* No port I/O on non-x86 architectures; hardware there is memory-mapped. */
uint8_t hal_port_in(uint16_t port) {
    (void)port;
    return 0;
}

void hal_port_out(uint16_t port, uint8_t val) {
    (void)port;
    (void)val;
}

uint16_t hal_port_in16(uint16_t port) {
    (void)port;
    return 0;
}

void hal_port_out16(uint16_t port, uint16_t val) {
    (void)port;
    (void)val;
}

#endif

uint32_t hal_mmio_read(volatile void* addr) {
    return *(volatile uint32_t*)addr;
}

void hal_mmio_write(volatile void* addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}
