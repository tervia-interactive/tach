#include <kernel/types.h>
/*
 * tach Operating System - Kernel Panic Handler
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <kernel/panic.h>
#include <kernel/types.h>
#include <hal/hal_console.h>
#include <hal/hal_cpu.h>

void kernel_panic(const char *message) {
    hal_console_putchar('\n');
    hal_console_putchar('P');
    hal_console_putchar('A');
    hal_console_putchar('N');
    hal_console_putchar('I');
    hal_console_putchar('C');
    hal_console_putchar(':');
    hal_console_putchar(' ');
    
    while (*message) {
        hal_console_putchar(*message++);
    }
    hal_console_putchar('\n');
    hal_console_putchar('S');
    hal_console_putchar('y');
    hal_console_putchar('s');
    hal_console_putchar('t');
    hal_console_putchar('e');
    hal_console_putchar('m');
    hal_console_putchar(' ');
    hal_console_putchar('h');
    hal_console_putchar('a');
    hal_console_putchar('l');
    hal_console_putchar('t');
    hal_console_putchar('e');
    hal_console_putchar('d');
    hal_console_putchar('.');
    hal_console_putchar('\n');
    
    while (1) {
        hal_cpu_halt();
    }
}
