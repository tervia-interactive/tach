/* tach - Minimal Freestanding printf Implementation */
#include <kernel/types.h>
#include <kernel/printf.h>

/* Divides a 64-bit value by a small base using shift/compare/subtract
 * binary long division, writing the quotient back through *value and
 * returning the remainder (the next digit). This intentionally avoids
 * the hardware 64-bit divide instruction: on i686 that instruction
 * doesn't exist and gcc instead calls __udivmoddi4() from libgcc, which
 * this -nostdlib freestanding kernel doesn't link against. Shifts,
 * compares and subtracts on a 64-bit value need no such helper on any
 * target, so this works identically on i686 and x86_64. */
static unsigned divmod_u64(uint64_t* value, unsigned base) {
    uint64_t quotient = 0;
    uint64_t remainder = 0;
    for (int bit = 63; bit >= 0; bit--) {
        remainder = (remainder << 1) | ((*value >> bit) & 1u);
        if (remainder >= base) {
            remainder -= base;
            quotient |= ((uint64_t)1 << bit);
        }
    }
    *value = quotient;
    return (unsigned)remainder;
}

static void print_uint(void (*putc)(char), uint64_t value, unsigned base,
                        int width, char pad) {
    char buf[24];
    int i = 0;
    static const char digits[] = "0123456789abcdef";

    do {
        buf[i++] = digits[divmod_u64(&value, base)];
    } while (value != 0);

    while (i < width) {
        buf[i++] = pad;
    }

    while (i--) {
        putc(buf[i]);
    }
}

static void print_int(void (*putc)(char), int64_t value, int width, char pad) {
    if (value < 0) {
        putc('-');
        if (width > 0) {
            width--;
        }
        print_uint(putc, (uint64_t)(-value), 10, width, pad);
    } else {
        print_uint(putc, (uint64_t)value, 10, width, pad);
    }
}

void kvprintf(void (*putc)(char), const char* fmt, va_list args) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            putc(*fmt);
            continue;
        }

        fmt++;

        char pad = ' ';
        int width = 0;
        int longflag = 0;

        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        while (*fmt == 'l') {
            longflag++;
            fmt++;
        }

        switch (*fmt) {
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) {
                    s = "(null)";
                }
                while (*s) {
                    putc(*s++);
                }
                break;
            }
            case 'c':
                putc((char)va_arg(args, int));
                break;
            case 'd':
            case 'i': {
                int64_t v = longflag ? va_arg(args, int64_t) : va_arg(args, int);
                print_int(putc, v, width, pad);
                break;
            }
            case 'u': {
                uint64_t v = longflag ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                print_uint(putc, v, 10, width, pad);
                break;
            }
            case 'x': {
                uint64_t v = longflag ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                print_uint(putc, v, 16, width, pad);
                break;
            }
            case 'p': {
                uintptr_t v = (uintptr_t)va_arg(args, void*);
                putc('0');
                putc('x');
                print_uint(putc, v, 16, (int)sizeof(uintptr_t) * 2, '0');
                break;
            }
            case '%':
                putc('%');
                break;
            case '\0':
                /* Trailing '%' at end of format string - stop. */
                return;
            default:
                putc('%');
                putc(*fmt);
                break;
        }
    }
}

void kprintf_to(void (*putc)(char), const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(putc, fmt, args);
    va_end(args);
}
