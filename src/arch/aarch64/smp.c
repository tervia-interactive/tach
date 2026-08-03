#include <hal/paging.h>
#include <hal/smp.h>
#include <kernel/percpu.h>
#include <mm/pmm.h>

#define PSCI_CPU_ON 0xc4000003u
#define PSCI_AFFINITY_INFO 0xc4000004u
static uint64_t g_mpidrs[MAX_CPUS];
static int g_cpu_count = 1;
static uintptr_t g_secondary_stacks[MAX_CPUS];
static long psci(uint64_t function, uint64_t a0, uint64_t a1, uint64_t a2) {
    register uint64_t x0 __asm__("x0") = function;
    register uint64_t x1 __asm__("x1") = a0;
    register uint64_t x2 __asm__("x2") = a1;
    register uint64_t x3 __asm__("x3") = a2;
    __asm__ volatile("hvc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x3) : "memory");
    return (long)x0;
}
static uint64_t current_mpidr(void) {
    uint64_t value; __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
    return value & 0xff00ffffffULL;
}
__attribute__((naked)) static void arm_secondary(
    uint64_t logical_cpu __attribute__((unused))) {
    __asm__ volatile(
        "adrp x1, g_secondary_stacks\n"
        "add x1, x1, :lo12:g_secondary_stacks\n"
        "ldr x1, [x1, x0, lsl #3]\n"
        "mov sp, x1\n"
        "b hal_smp_secondary_entry\n");
}
int arch_smp_init(void) {
    uint64_t boot = current_mpidr(); g_mpidrs[0] = boot; g_cpu_count = 1;
    for (uint64_t affinity = 0; affinity < 64 && g_cpu_count < MAX_CPUS;
         affinity++) {
        if (affinity == boot) continue;
        long status = psci(PSCI_AFFINITY_INFO, affinity, 0, 0);
        if (status == 0 || status == 1) g_mpidrs[g_cpu_count++] = affinity;
    }
    return g_cpu_count;
}
int arch_smp_cpu_count(void) { return g_cpu_count; }
uint32_t arch_smp_current_cpu(void) {
    uint64_t id = current_mpidr();
    for (int cpu = 0; cpu < g_cpu_count; cpu++)
        if (g_mpidrs[cpu] == id) return (uint32_t)cpu;
    return 0;
}
int arch_smp_boot_secondary(uint32_t cpu, void* entry) {
    (void)entry;
    if (!cpu || cpu >= (uint32_t)g_cpu_count) return -1;
    void* stack = pmm_alloc_pages(4);
    if (!stack) return -1;
    g_secondary_stacks[cpu] = (uintptr_t)stack + 4 * PAGE_SIZE;
    return (int)psci(PSCI_CPU_ON, g_mpidrs[cpu],
                     (uint64_t)(uintptr_t)arm_secondary, cpu);
}
void arch_smp_secondary_init(uint32_t cpu) {
    (void)cpu;
    extern void exception_vector_el1(void);
    extern void gic_init(void);
    uintptr_t vector = (uintptr_t)exception_vector_el1;
    __asm__ volatile("msr vbar_el1, %0; isb" :: "r"(vector) : "memory");
    arch_paging_switch(arch_paging_kernel_root());
    gic_init();
}
void arch_smp_send_ipi(uint32_t cpu, uint32_t vector) {
    (void)vector;
    if (cpu >= (uint32_t)g_cpu_count || cpu >= 8) return;
    *(volatile uint32_t*)(uintptr_t)(0x08000000u + 0x0f00u) =
        (1u << (16 + cpu));
}
