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

/* Set by serial_init() only when a real UART answers the loopback test.
 * Without one (VirtualBox/VMware without a serial port, many PCs), every
 * read of COM1 returns 0xFF, which looks like "data ready" forever. */
static int g_serial_present;

void serial_init(void) {
    g_serial_present = 0;
    hal_port_out(COM1_PORT + 1, 0x00);    /* disable interrupts */
    hal_port_out(COM1_PORT + 3, 0x80);    /* enable DLAB (set baud divisor) */
    hal_port_out(COM1_PORT + 0, 0x03);    /* divisor low byte: 38400 baud */
    hal_port_out(COM1_PORT + 1, 0x00);    /* divisor high byte */
    hal_port_out(COM1_PORT + 3, 0x03);    /* 8 bits, no parity, 1 stop bit */
    hal_port_out(COM1_PORT + 2, 0xC7);    /* enable + clear FIFO, 14-byte threshold */

    /* Loopback self-test: a floating bus never echoes the byte back. */
    hal_port_out(COM1_PORT + 4, 0x1E);    /* loopback mode, OUT1/OUT2 set */
    hal_port_out(COM1_PORT + 0, 0xAE);
    if (hal_port_in(COM1_PORT + 0) != 0xAE ||
        hal_port_in(COM1_PORT + 5) == 0xFF) {
        return;                           /* no UART: stay disabled */
    }
    hal_port_out(COM1_PORT + 4, 0x0B);    /* IRQs disabled, RTS/DSR set */
    g_serial_present = 1;
}

static int transmit_empty(void) {
    return hal_port_in(COM1_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    if (!g_serial_present) return;
    if (c == '\n') {
        while (!transmit_empty()) {}
        hal_port_out(COM1_PORT, '\r');
    }
    while (!transmit_empty()) {}
    hal_port_out(COM1_PORT, (uint8_t)c);
}

int serial_available(void) {
    if (!g_serial_present) return 0;
    return hal_port_in(COM1_PORT + 5) & 0x01;
}

char serial_getchar(void) {
    if (!g_serial_present) return 0;
    while (!serial_available()) {}
    return (char)hal_port_in(COM1_PORT);
}
