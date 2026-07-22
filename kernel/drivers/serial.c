/*
 * tach - A minimal open source Operating System
 * Copyright 2026 Tervia Interactive™
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "serial.h"
#include "kernel.h"

#ifdef ARCH_X86

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    /* Disable interrupts */
    outb(SERIAL_COM1 + 1, 0x00);
    
    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_COM1 + 3, 0x80);
    
    /* Set divisor to 3 (lo byte) 38400 baud */
    outb(SERIAL_COM1 + 0, 0x03);
    outb(SERIAL_COM1 + 1, 0x00);
    
    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_COM1 + 3, 0x03);
    
    /* Enable FIFO */
    outb(SERIAL_COM1 + 2, 0xC7);
    
    /* Enable IRQs */
    outb(SERIAL_COM1 + 1, 0x01);
}

int serial_is_transmit_empty(void) {
    return inb(SERIAL_COM1 + 5) & 0x20;
}

void serial_write_char(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(SERIAL_COM1, c);
}

void serial_write(const char* str) {
    while (*str) {
        serial_write_char(*str++);
    }
}

#else
/* Stub implementations for non-x86 architectures */
void serial_init(void) {
    /* No-op for ARM and other architectures */
}

int serial_is_transmit_empty(void) {
    return 1;
}

void serial_write_char(char c) {
    /* No-op for ARM and other architectures */
}

void serial_write(const char* str) {
    /* No-op for ARM and other architectures */
}

#endif
