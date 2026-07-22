/*
 * tach Operating System - Kernel Main Entry Point
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <kernel/types.h>
#include <kernel/panic.h>
#include <kernel/version.h>
#include <hal/hal_console.h>
#include <hal/hal_cpu.h>

void kernel_main(void) {
    hal_console_early_init();
    
    hal_console_putchar('\n');
    hal_console_putchar('\n');
    hal_console_putchar('W');
    hal_console_putchar('e');
    hal_console_putchar('l');
    hal_console_putchar('c');
    hal_console_putchar('o');
    hal_console_putchar('m');
    hal_console_putchar('e');
    hal_console_putchar(' ');
    hal_console_putchar('t');
    hal_console_putchar('o');
    hal_console_putchar(' ');
    hal_console_putchar('t');
    hal_console_putchar('a');
    hal_console_putchar('c');
    hal_console_putchar('h');
    hal_console_putchar(' ');
    hal_console_putchar('O');
    hal_console_putchar('S');
    hal_console_putchar('\n');
    hal_console_putchar('V');
    hal_console_putchar('e');
    hal_console_putchar('r');
    hal_console_putchar('s');
    hal_console_putchar('i');
    hal_console_putchar('o');
    hal_console_putchar('n');
    hal_console_putchar(':');
    hal_console_putchar(' ');
    hal_console_putchar(TACH_VERSION_MAJOR + '0');
    hal_console_putchar('.');
    hal_console_putchar(TACH_VERSION_MINOR + '0');
    hal_console_putchar('.');
    hal_console_putchar(TACH_VERSION_PATCH + '0');
    hal_console_putchar('-');
    hal_console_putchar('d');
    hal_console_putchar('e');
    hal_console_putchar('v');
    hal_console_putchar('\n');
    hal_console_putchar('\n');
    hal_console_putchar('S');
    hal_console_putchar('y');
    hal_console_putchar('s');
    hal_console_putchar('t');
    hal_console_putchar('e');
    hal_console_putchar('m');
    hal_console_putchar(' ');
    hal_console_putchar('i');
    hal_console_putchar('n');
    hal_console_putchar('i');
    hal_console_putchar('t');
    hal_console_putchar('i');
    hal_console_putchar('a');
    hal_console_putchar('l');
    hal_console_putchar('i');
    hal_console_putchar('z');
    hal_console_putchar('e');
    hal_console_putchar('d');
    hal_console_putchar(' ');
    hal_console_putchar('s');
    hal_console_putchar('u');
    hal_console_putchar('c');
    hal_console_putchar('c');
    hal_console_putchar('e');
    hal_console_putchar('s');
    hal_console_putchar('s');
    hal_console_putchar('f');
    hal_console_putchar('u');
    hal_console_putchar('l');
    hal_console_putchar('l');
    hal_console_putchar('y');
    hal_console_putchar('.');
    hal_console_putchar('\n');
    
    while (1) {
        hal_cpu_halt();
    }
}
