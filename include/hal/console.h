/* tach - HAL Console Header */

#ifndef _HAL_CONSOLE_H
#define _HAL_CONSOLE_H

#include "kernel/types.h"

void hal_console_early_init(void);
void hal_console_putchar(char c);
void hal_console_write(const char* str, size_t len);
void hal_console_clear(void);

/* Serialize multi-character console output so concurrent writers (kernel
 * log on one CPU, userspace write() on another) cannot interleave inside a
 * line. Not re-entrant; panic paths must keep using hal_console_putchar(). */
void hal_console_lock(void);
void hal_console_unlock(void);
int hal_console_input_available(void);
char hal_console_getchar(void);

#endif /* _HAL_CONSOLE_H */
