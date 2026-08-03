/*
 * drivers/char/serial.h - UART serial driver interface
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _DRIVERS_CHAR_SERIAL_H
#define _DRIVERS_CHAR_SERIAL_H

#include <kernel/types.h>

void serial_init(void);
void serial_putchar(char c);
char serial_getchar(void);
int serial_available(void);

int serial_pl011_init(uintptr_t base);
void serial_pl011_putchar(char c);
char serial_pl011_getchar(void);
int serial_pl011_available(void);
void serial_sbi_init(void);
void serial_sbi_putchar(char c);
char serial_sbi_getchar(void);
int serial_sbi_available(void);

#endif /* _DRIVERS_CHAR_SERIAL_H */
