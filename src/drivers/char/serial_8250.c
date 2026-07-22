/*
 * drivers/char/serial_8250.c - 16550/8250 UART driver (x86 COM1)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Implements the API declared in serial.h (serial_init/serial_putchar/
 * serial_getchar/serial_available). This is the only serial
 * implementation wired into the build for i686/x86_64 right now, so it
 * intentionally uses fixed COM1 I/O ports rather than taking a port
 * argument.
 */
#include <drivers/char/serial.h>
#include <hal/io.h>

#define COM1_PORT 0x3F8

void serial_init(void) {
    hal_port_out(COM1_PORT + 1, 0x00);    /* disable interrupts */
    hal_port_out(COM1_PORT + 3, 0x80);    /* enable DLAB (set baud divisor) */
    hal_port_out(COM1_PORT + 0, 0x03);    /* divisor low byte: 38400 baud */
    hal_port_out(COM1_PORT + 1, 0x00);    /* divisor high byte */
    hal_port_out(COM1_PORT + 3, 0x03);    /* 8 bits, no parity, 1 stop bit */
    hal_port_out(COM1_PORT + 2, 0xC7);    /* enable + clear FIFO, 14-byte threshold */
    hal_port_out(COM1_PORT + 4, 0x0B);    /* IRQs disabled, RTS/DSR set */
}

static int transmit_empty(void) {
    return hal_port_in(COM1_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    if (c == '\n') {
        while (!transmit_empty()) {}
        hal_port_out(COM1_PORT, '\r');
    }
    while (!transmit_empty()) {}
    hal_port_out(COM1_PORT, (uint8_t)c);
}

int serial_available(void) {
    return hal_port_in(COM1_PORT + 5) & 0x01;
}

char serial_getchar(void) {
    while (!serial_available()) {}
    return (char)hal_port_in(COM1_PORT);
}
