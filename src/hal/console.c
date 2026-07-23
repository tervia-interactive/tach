#include <kernel/types.h>
/*
 * HAL Console Dispatcher
 * Copyright 2026 Tervia Interactive™
 */

#include <hal/console.h>

#if defined(__i386__) || defined(__x86_64__)
#include <drivers/char/serial.h>
#include <drivers/video/vga_text.h>

void hal_console_early_init(void) {
    serial_init();
    vga_init();
}

void hal_console_putchar(char c) {
    /* VGA is the primary, on-screen console. Serial is mirrored too so
     * headless QEMU runs (-nographic / -serial stdio) still capture the
     * boot log. */
    vga_putc(c);
    serial_putchar(c);
}

void hal_console_write(const char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        hal_console_putchar(str[i]);
    }
}

void hal_console_clear(void) {
    vga_clear();
}

#else

void hal_console_early_init(void) {
    /* No console driver wired up yet for this architecture. */
}

void hal_console_putchar(char c) {
    (void)c;
}

void hal_console_write(const char* str, size_t len) {
    (void)str;
    (void)len;
}

void hal_console_clear(void) {
}

#endif
