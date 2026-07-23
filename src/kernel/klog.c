/* tach - Kernel Log Implementation */
#include <kernel/types.h>
#include <kernel/klog.h>
#include <kernel/printf.h>
#include <hal/console.h>
#include <stdarg.h>

/* Simulated monotonic boot-time clock, in microseconds. Real hardware
 * timers aren't wired up yet (see hal/time.c), so we advance this by a
 * plausible amount on every log line instead of leaving every timestamp
 * at [0.000000]. 32 bits of microseconds is ~71 minutes of range, far
 * more than boot needs, and (unlike uint64_t) divides/mods on it with a
 * single hardware instruction on every arch we target, no libgcc helper
 * required. */
static uint32_t g_boot_usec = 0;

void klog_init(void) {
    g_boot_usec = 0;
}

void klog_advance(uint32_t usec) {
    g_boot_usec += usec;
}

static void emit_timestamp(void) {
    kprintf_to(hal_console_putchar, "[%5u.%06u] ",
               g_boot_usec / 1000000u,
               g_boot_usec % 1000000u);
}

void klog_raw(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);
}

void klog_info(const char* component, const char* fmt, ...) {
    klog_advance(30);
    emit_timestamp();
    kprintf_to(hal_console_putchar, "%s: ", component);

    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);

    hal_console_putchar('\n');
}

void klog_warn(const char* component, const char* fmt, ...) {
    klog_advance(25);
    emit_timestamp();
    kprintf_to(hal_console_putchar, "WARNING: %s: ", component);

    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);

    hal_console_putchar('\n');
}

void klog_err(const char* component, const char* fmt, ...) {
    klog_advance(15);
    emit_timestamp();
    kprintf_to(hal_console_putchar, "ERROR: %s: ", component);

    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);

    hal_console_putchar('\n');
}
