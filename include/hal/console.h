/* tach - HAL Console Header */

#ifndef _HAL_CONSOLE_H
#define _HAL_CONSOLE_H

#include "kernel/types.h"

void hal_console_early_init(void);
void hal_console_putchar(char c);
void hal_console_write(const char* str, size_t len);
void hal_console_clear(void);
int hal_console_input_available(void);
char hal_console_getchar(void);

#endif /* _HAL_CONSOLE_H */
