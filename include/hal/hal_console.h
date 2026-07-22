/* tach Operating System - HAL Console Header */

#ifndef _HAL_CONSOLE_H
#define _HAL_CONSOLE_H

#include "kernel/types.h"

void hal_console_early_init(void);
void hal_console_putchar(char c);
void hal_console_write(const char* str, size_t len);
void hal_console_clear(void);

#endif /* _HAL_CONSOLE_H */
