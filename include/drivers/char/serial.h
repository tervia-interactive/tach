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

#endif /* _DRIVERS_CHAR_SERIAL_H */
