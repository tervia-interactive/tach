/**
 * @file printf.c
 * @brief Minimal printf implementation for tach
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include <stdarg.h>
#include <stddef.h>
#include "tach.h"

static int print_number(int num, int is_signed, int (*putc_fn)(int)) {
    char buf[32];
    int i = 0;
    int count = 0;
    unsigned int unum;
    
    if (is_signed && num < 0) {
        putc_fn('-');
        count++;
        unum = (unsigned int)(-num);
    } else {
        unum = (unsigned int)num;
    }
    
    do {
        buf[i++] = '0' + (unum % 10);
        unum /= 10;
    } while (unum > 0);
    
    while (i > 0) {
        putc_fn(buf[--i]);
        count++;
    }
    
    return count;
}

static int print_hex(unsigned int num, int uppercase, int (*putc_fn)(int)) {
    char buf[16];
    int i = 0;
    int count = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    do {
        buf[i++] = digits[num & 0xF];
        num >>= 4;
    } while (num > 0);
    
    while (i > 0) {
        putc_fn(buf[--i]);
        count++;
    }
    
    return count;
}

static int print_string(const char *str, int (*putc_fn)(int)) {
    int count = 0;
    
    if (!str) {
        str = "(null)";
    }
    
    while (*str) {
        putc_fn(*str++);
        count++;
    }
    
    return count;
}

static int putchar_wrapper(int c) {
    write(STDOUT_FILENO, &c, 1);
    return c;
}

int vprintf(const char *format, va_list ap) {
    int count = 0;
    char c;
    
    while ((c = *format++) != '\0') {
        if (c == '%') {
            c = *format++;
            
            switch (c) {
                case 'd':
                case 'i':
                    count += print_number(va_arg(ap, int), 1, putchar_wrapper);
                    break;
                case 'u':
                    count += print_number(va_arg(ap, unsigned int), 0, putchar_wrapper);
                    break;
                case 'x':
                    count += print_hex(va_arg(ap, unsigned int), 0, putchar_wrapper);
                    break;
                case 'X':
                    count += print_hex(va_arg(ap, unsigned int), 1, putchar_wrapper);
                    break;
                case 'c':
                    putchar_wrapper(va_arg(ap, int));
                    count++;
                    break;
                case 's':
                    count += print_string(va_arg(ap, char *), putchar_wrapper);
                    break;
                case '%':
                    putchar_wrapper('%');
                    count++;
                    break;
                default:
                    putchar_wrapper('%');
                    putchar_wrapper(c);
                    count += 2;
                    break;
            }
        } else {
            putchar_wrapper(c);
            count++;
        }
    }
    
    return count;
}

int printf(const char *format, ...) {
    va_list ap;
    int count;
    
    va_start(ap, format);
    count = vprintf(format, ap);
    va_end(ap);
    
    return count;
}

int puts(const char *s) {
    int count = 0;
    
    count += print_string(s, putchar_wrapper);
    putchar_wrapper('\n');
    count++;
    
    return count;
}

int putchar(int c) {
    return putchar_wrapper(c);
}
