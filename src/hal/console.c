#include <kernel/types.h>
/*
 * HAL Console Dispatcher
 * Copyright 2026 Tervia Interactive™
 */

#include <hal/console.h>

#if defined(__i386__) || defined(__x86_64__)
#include <drivers/char/serial.h>
#include <drivers/input/keyboard.h>
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

#elif defined(__arm__) || defined(__aarch64__)

#include <drivers/char/serial.h>
#define PL011_VIRT_BASE ((uintptr_t)0x09000000u)

void hal_console_early_init(void) { (void)serial_pl011_init(PL011_VIRT_BASE); }
void hal_console_putchar(char c) { serial_pl011_putchar(c); }
void hal_console_write(const char* str, size_t len) {
    while (len--) serial_pl011_putchar(*str++);
}
void hal_console_clear(void) { hal_console_write("\033[2J\033[H", 7); }
int hal_console_input_available(void) { return serial_pl011_available(); }
char hal_console_getchar(void) { return serial_pl011_getchar(); }

#elif defined(__riscv)

#include <drivers/char/serial.h>
void hal_console_early_init(void) { serial_sbi_init(); }
void hal_console_putchar(char c) { serial_sbi_putchar(c); }
void hal_console_write(const char* str, size_t len) {
    while (len--) serial_sbi_putchar(*str++);
}
void hal_console_clear(void) { hal_console_write("\033[2J\033[H", 7); }
int hal_console_input_available(void) { return serial_sbi_available(); }
char hal_console_getchar(void) { return serial_sbi_getchar(); }

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

int hal_console_input_available(void) {
    return 0;
}

char hal_console_getchar(void) {
    return 0;
}

#endif
