#include <hal/io.h>
uint8_t hal_port_in(uint16_t port) {(void)port; return 0;}
void hal_port_out(uint16_t port, uint8_t val) {(void)port;(void)val;}
uint32_t hal_mmio_read(uintptr_t addr) {(void)addr; return 0;}
void hal_mmio_write(uintptr_t addr, uint32_t val) {(void)addr;(void)val;}
