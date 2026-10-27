#include <kernel/types.h>
/*
 * HAL Console Dispatcher
 * Copyright 2026 Tervia Interactive™
 */

#include <hal/console.h>
#include <kernel/spinlock.h>

#if (defined(__i386__) || defined(__x86_64__)) && !defined(TACH_HOST_TEST)
#include <drivers/char/serial.h>
#include <drivers/input/keyboard.h>
#include <drivers/video/vga_text.h>

static spinlock_t g_console_lock = SPINLOCK_INIT;

void hal_console_lock(void) {
    spinlock_lock(&g_console_lock);
}

void hal_console_unlock(void) {
    spinlock_unlock(&g_console_lock);
}

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
    hal_console_lock();
    for (size_t i = 0; i < len; i++) {
        hal_console_putchar(str[i]);
    }
    hal_console_unlock();
}

void hal_console_clear(void) {
    vga_clear();
}

int hal_console_input_available(void) {
    return serial_available() || keyboard_available();
}

char hal_console_getchar(void) {
    while (1) {
        if (serial_available()) {
            return serial_getchar();
        }
        if (keyboard_available()) {
            return keyboard_readchar();
        }
    }
}

#else

void hal_console_lock(void) {
}

void hal_console_unlock(void) {
}

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

int hal_console_input_available(void) {
    return 0;
}

char hal_console_getchar(void) {
    return 0;
}

#endif
