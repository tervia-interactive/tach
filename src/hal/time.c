/* tach - Periodic architecture timer driving scheduler preemption. */
#include <kernel/types.h>
#include <hal/cpu.h>
#include <hal/irq.h>
#include <hal/time.h>
#include <hal/smp.h>
#include <proc/scheduler.h>

#define TACH_TIMER_HZ 100u

static volatile uint64_t g_timer_ticks;
static uint64_t g_timer_frequency = TACH_TIMER_HZ;
#if !defined(__x86_64__) && !defined(__i386__) || defined(TACH_HOST_TEST)
static uint64_t g_timer_interval = 1;
#endif

#if defined(__aarch64__) || defined(__arm__)
extern void gic_init(void);
extern void gic_enable_irq(uint32_t irq);
#endif

#if defined(__x86_64__) || defined(__i386__)
static inline void timer_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}
#endif

#if defined(__riscv)
#if __riscv_xlen == 64
extern void aclint_set_timer(uint64_t deadline);
#endif
static uint64_t riscv_read_time(void) {
#if __riscv_xlen == 64
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
#else
    uint32_t high1, low, high2;
    do {
        __asm__ volatile("rdtimeh %0" : "=r"(high1));
        __asm__ volatile("rdtime %0" : "=r"(low));
        __asm__ volatile("rdtimeh %0" : "=r"(high2));
    } while (high1 != high2);
    return ((uint64_t)high1 << 32) | low;
#endif
}

static void riscv_set_timer(uint64_t deadline) {
#if __riscv_xlen == 64
    aclint_set_timer(deadline);
#else
    register uintptr_t a0 __asm__("a0") = (uintptr_t)deadline;
    register uintptr_t a1 __asm__("a1") = (uintptr_t)(deadline >> 32);
    register uintptr_t a6 __asm__("a6") = 0;
    register uintptr_t a7 __asm__("a7") = 0x54494d45u;
    __asm__ volatile("ecall" : "+r"(a0)
                     : "r"(a1), "r"(a6), "r"(a7) : "memory");
#endif
}
#endif

static void timer_rearm(void) {
#if defined(__aarch64__)
    uint64_t interval = g_timer_interval;
    __asm__ volatile("msr cntp_tval_el0, %0; isb" :: "r"(interval));
#elif defined(__arm__)
    uint32_t interval = (uint32_t)g_timer_interval;
    __asm__ volatile("mcr p15, 0, %0, c14, c2, 0" :: "r"(interval));
#elif defined(__riscv)
    riscv_set_timer(riscv_read_time() + g_timer_interval);
#endif
}

#ifndef TACH_HOST_TEST
static void timer_irq(void* argument) {
    (void)argument;
    hal_timer_interrupt();
}
#endif

void hal_timer_init(void) {
    g_timer_ticks = 0;
#ifdef TACH_HOST_TEST
    g_timer_frequency = TACH_TIMER_HZ;
    g_timer_interval = 1;
#elif defined(__x86_64__) || defined(__i386__)
    const uint32_t pit_frequency = 1193182u;
    uint16_t divisor = (uint16_t)(pit_frequency / TACH_TIMER_HZ);
    timer_outb(0x43, 0x36);
    timer_outb(0x40, (uint8_t)(divisor & 0xff));
    timer_outb(0x40, (uint8_t)(divisor >> 8));
    g_timer_frequency = TACH_TIMER_HZ;
    (void)hal_irq_register(0, timer_irq, NULL);
#elif defined(__aarch64__)
    uint64_t counter_frequency;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(counter_frequency));
    g_timer_frequency = TACH_TIMER_HZ;
    g_timer_interval = counter_frequency / TACH_TIMER_HZ;
    if (!g_timer_interval) g_timer_interval = 1;
    gic_init();
    (void)hal_irq_register(30, timer_irq, NULL);
    gic_enable_irq(30);
    timer_rearm();
    uint64_t control = 1;
    __asm__ volatile("msr cntp_ctl_el0, %0; isb" :: "r"(control));
#elif defined(__arm__)
    uint32_t counter_frequency;
    __asm__ volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(counter_frequency));
    g_timer_frequency = TACH_TIMER_HZ;
    g_timer_interval = counter_frequency / TACH_TIMER_HZ;
    if (!g_timer_interval) g_timer_interval = 1;
    gic_init();
    (void)hal_irq_register(30, timer_irq, NULL);
    gic_enable_irq(30);
    timer_rearm();
    uint32_t control = 1;
    __asm__ volatile("mcr p15, 0, %0, c14, c2, 1" :: "r"(control));
#elif defined(__riscv)
    g_timer_frequency = TACH_TIMER_HZ;
    g_timer_interval = 10000000u / TACH_TIMER_HZ;
    (void)hal_irq_register(5, timer_irq, NULL);
    uintptr_t timer_mask = (uintptr_t)1 << 5;
    __asm__ volatile("csrs sie, %0" :: "r"(timer_mask) : "memory");
    timer_rearm();
#endif
}

void hal_timer_init_secondary(void) {
#ifdef TACH_HOST_TEST
    return;
#elif defined(__x86_64__) || defined(__i386__)
    extern void x86_lapic_timer_init(void);
    x86_lapic_timer_init();
#elif defined(__aarch64__) || defined(__arm__)
    gic_init();
    gic_enable_irq(30);
    timer_rearm();
#if defined(__aarch64__)
    uint64_t control = 1;
    __asm__ volatile("msr cntp_ctl_el0, %0; isb" :: "r"(control));
#else
    uint32_t control = 1;
    __asm__ volatile("mcr p15, 0, %0, c14, c2, 1" :: "r"(control));
#endif
#elif defined(__riscv)
    uintptr_t timer_mask = (uintptr_t)1 << 5;
    __asm__ volatile("csrs sie, %0" :: "r"(timer_mask) : "memory");
    timer_rearm();
#endif
}

void hal_timer_interrupt(void) {
    if (hal_smp_current_cpu() == 0) g_timer_ticks++;
    timer_rearm();
    scheduler_tick();
}

uint64_t hal_timer_get_ticks(void) { return g_timer_ticks; }
uint64_t hal_timer_get_frequency(void) { return g_timer_frequency; }

static uint64_t timer_divide_u64(uint64_t dividend, uint32_t divisor) {
    uint64_t quotient = 0;
    uint64_t remainder = 0;
    for (int bit = 63; bit >= 0; bit--) {
        remainder = (remainder << 1) | ((dividend >> bit) & 1u);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (uint64_t)1 << bit;
        }
    }
    return quotient;
}

void hal_timer_sleep(uint64_t milliseconds) {
    uint64_t ticks = timer_divide_u64(milliseconds + 9u, 10u);
    uint64_t deadline = g_timer_ticks + ticks;
    while (g_timer_ticks < deadline) {
#ifdef TACH_HOST_TEST
        hal_timer_interrupt();
#else
        scheduler_yield();
        hal_cpu_halt();
#endif
    }
}
