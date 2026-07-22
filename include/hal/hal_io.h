/* tach Operating System - HAL IO Header */

#ifndef _HAL_IO_H
#define _HAL_IO_H

#include "kernel/types.h"

uint8_t hal_port_in(uint16_t port);
void hal_port_out(uint16_t port, uint8_t val);
uint16_t hal_port_in16(uint16_t port);
void hal_port_out16(uint16_t port, uint16_t val);
uint32_t hal_mmio_read(volatile void* addr);
void hal_mmio_write(volatile void* addr, uint32_t val);

#endif /* _HAL_IO_H */
