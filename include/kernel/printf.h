/* tach - Minimal Freestanding printf Header */
/*
 * A tiny, allocation-free printf engine for kernel space. Supports the
 * subset of format specifiers the kernel actually needs: %s %c %d %i %u
 * %x %p %%, an 'l' length modifier for 64-bit values, a '0' zero-pad flag,
 * and a decimal field width (e.g. "%5u", "%06u"). Nothing else — no
 * floating point, no positional args.
 */

#ifndef _KERNEL_PRINTF_H
#define _KERNEL_PRINTF_H

#include "kernel/types.h"
#include <stdarg.h>

/* Formats fmt/args, emitting one character at a time via putc. */
void kvprintf(void (*putc)(char), const char* fmt, va_list args);

/* Convenience varargs wrapper around kvprintf. */
void kprintf_to(void (*putc)(char), const char* fmt, ...);

#endif /* _KERNEL_PRINTF_H */
