/* tach - Architecture-neutral SMP startup and secondary CPU rendezvous. */
#include <kernel/types.h>
#include <kernel/percpu.h>
#include <hal/smp.h>
#include <hal/cpu.h>
#include <hal/irq.h>
#include <hal/time.h>
#include <proc/scheduler.h>

static volatile bool g_cpu_online[MAX_CPUS];
static bool g_smp_initialized;
static int g_active_cpu_count = 1;

__attribute__((weak)) int arch_smp_init(void) { return 1; }
__attribute__((weak)) int arch_smp_boot_secondary(uint32_t cpu_id, void* entry) {
    (void)cpu_id; (void)entry; return -1;
}
__attribute__((weak)) int arch_smp_cpu_count(void) { return 1; }
__attribute__((weak)) uint32_t arch_smp_current_cpu(void) { return 0; }
__attribute__((weak)) void arch_smp_send_ipi(uint32_t cpu_id, uint32_t vector) {
    (void)cpu_id; (void)vector;
}
__attribute__((weak)) void arch_smp_secondary_init(uint32_t cpu_id) {
    (void)cpu_id;
}

int hal_smp_boot_secondary(uint32_t cpu_id, void* entry) {
    if (!cpu_id || cpu_id >= (uint32_t)arch_smp_cpu_count()) return -1;
    return arch_smp_boot_secondary(cpu_id, entry);
}

int hal_smp_cpu_count(void) {
    int count = g_smp_initialized ? g_active_cpu_count : arch_smp_cpu_count();
    if (count < 1) count = 1;
    if (count > MAX_CPUS) count = MAX_CPUS;
    return count;
}

uint32_t hal_smp_current_cpu(void) {
    if (!g_smp_initialized) return 0;
    uint32_t cpu = arch_smp_current_cpu();
    return cpu < MAX_CPUS ? cpu : 0;
}

bool hal_smp_cpu_online(uint32_t cpu_id) {
    return cpu_id < MAX_CPUS && g_cpu_online[cpu_id];
}

void hal_smp_send_ipi(uint32_t cpu_id, uint32_t vector) {
    arch_smp_send_ipi(cpu_id, vector);
}

void hal_smp_init(void) {
    for (size_t i = 0; i < MAX_CPUS; i++) g_cpu_online[i] = false;
    g_cpu_online[0] = true;
    (void)arch_smp_init();
    g_smp_initialized = true;
    g_active_cpu_count = 1;
    int detected = arch_smp_cpu_count();
    if (detected > MAX_CPUS) detected = MAX_CPUS;
    for (int cpu = 1; cpu < detected; cpu++) {
        if (hal_smp_boot_secondary((uint32_t)cpu,
                                   (void*)hal_smp_secondary_entry) < 0)
            break;
        for (uint32_t wait = 0; wait < 10000000u && !g_cpu_online[cpu]; wait++)
            __asm__ volatile("" ::: "memory");
        if (!g_cpu_online[cpu]) break;
        g_active_cpu_count++;
    }
}

void hal_smp_secondary_entry(uint32_t cpu_id) {
    arch_smp_secondary_init(cpu_id);
    scheduler_init_cpu(cpu_id);
    g_cpu_online[cpu_id] = true;
    hal_timer_init_secondary();
    hal_irq_enable();
    for (;;) {
        scheduler_yield();
        hal_cpu_halt();
    }
}
