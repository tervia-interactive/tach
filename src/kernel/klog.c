/* tach - Kernel Log Implementation */
#include <kernel/types.h>
#include <kernel/klog.h>
#include <kernel/printf.h>
#include <hal/console.h>
#include <stdarg.h>

/* Monotonic early boot-time clock, in microseconds. It stays independent
 * of the scheduler timer so logging works before interrupts are enabled.
 * Keeping it 32-bit also avoids freestanding libgcc division helpers. */
static uint32_t g_boot_usec = 0;

void klog_init(void) {
    g_boot_usec = 0;
}

void klog_advance(uint32_t usec) {
    g_boot_usec += usec;
}

/* Weak: freestanding host tests link klog without the timer. */
extern uint64_t hal_timer_get_ticks(void) __attribute__((weak));

/* Once the 100 Hz timer runs, report real elapsed time; before that only a
 * synthetic counter that keeps log lines ordered is available. */
static void emit_timestamp(void) {
    uint32_t usec = g_boot_usec;
    if (hal_timer_get_ticks) {
        uint64_t ticks = hal_timer_get_ticks();
        if (ticks) usec = (uint32_t)(ticks * 10000u);
    }
    kprintf_to(hal_console_putchar, "[%5u.%06u] ",
               usec / 1000000u, usec % 1000000u);
}

void klog_raw(const char* fmt, ...) {
    hal_console_lock();
    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);
    hal_console_unlock();
}

void klog_info(const char* component, const char* fmt, ...) {
    hal_console_lock();
    klog_advance(30);
    emit_timestamp();
    kprintf_to(hal_console_putchar, "%s: ", component);

    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);

    hal_console_putchar('\n');
    hal_console_unlock();
}

void klog_warn(const char* component, const char* fmt, ...) {
    hal_console_lock();
    klog_advance(25);
    emit_timestamp();
    kprintf_to(hal_console_putchar, "WARNING: %s: ", component);

    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);

    hal_console_putchar('\n');
    hal_console_unlock();
}

void klog_err(const char* component, const char* fmt, ...) {
    hal_console_lock();
    klog_advance(15);
    emit_timestamp();
    kprintf_to(hal_console_putchar, "ERROR: %s: ", component);

    va_list args;
    va_start(args, fmt);
    kvprintf(hal_console_putchar, fmt, args);
    va_end(args);

    hal_console_putchar('\n');
    hal_console_unlock();
}
