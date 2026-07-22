#include <kernel/types.h>
/*
 * HAL Console Dispatcher
 * Copyright 2026 Tervia Interactive™
 */

#include <hal/hal_console.h>

void hal_console_early_init(void) {
    /* Architecture-specific early console init */
}

void hal_console_putchar(char c) {
    /* Architecture-specific putchar implementation */
    (void)c;
}
