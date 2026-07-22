#include <kernel/types.h>
/*
 * HAL Console Dispatcher
 * Copyright 2026 Tervia Interactive™
 */

#include <hal/console.h>

#if defined(__i386__) || defined(__x86_64__)
#include <drivers/char/serial.h>

void hal_console_early_init(void) {
    serial_init();
}

void hal_console_putchar(char c) {
    serial_putchar(c);
}

#else

void hal_console_early_init(void) {
    /* No console driver wired up yet for this architecture. */
}

void hal_console_putchar(char c) {
    (void)c;
}

#endif
