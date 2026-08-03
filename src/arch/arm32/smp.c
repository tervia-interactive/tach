#include <hal/paging.h>
#include <hal/smp.h>
#include <kernel/percpu.h>
#include <mm/pmm.h>

#define PSCI_CPU_ON 0x84000003u
#define PSCI_AFFINITY_INFO 0x84000004u
static uint32_t g_mpidrs[MAX_CPUS];
static int g_cpu_count = 1;
static uintptr_t g_secondary_stacks[MAX_CPUS] __attribute__((used));
static int32_t psci(uint32_t function, uint32_t a0, uint32_t a1, uint32_t a2) {
    register uint32_t r0 __asm__("r0") = function;
    register uint32_t r1 __asm__("r1") = a0;
    register uint32_t r2 __asm__("r2") = a1;
    register uint32_t r3 __asm__("r3") = a2;
    __asm__ volatile(".inst 0xe1400070" : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3) : "memory");
    return (int32_t)r0;
}
static uint32_t current_mpidr(void) {
    uint32_t value;
    __asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(value));
    return value & 0x00ffffffu;
}
__attribute__((naked)) static void arm_secondary(
    uint32_t logical_cpu __attribute__((unused))) {
    __asm__ volatile(
        "ldr r1, =g_secondary_stacks\n"
        "ldr sp, [r1, r0, lsl #2]\n"
        "b hal_smp_secondary_entry\n");
}
int arch_smp_init(void) {
    uint32_t boot = current_mpidr(); g_mpidrs[0] = boot; g_cpu_count = 1;
    for (uint32_t affinity = 0; affinity < 32 && g_cpu_count < MAX_CPUS;
         affinity++) {
        if (affinity == boot) continue;
        int32_t status = psci(PSCI_AFFINITY_INFO, affinity, 0, 0);
        if (status == 0 || status == 1) g_mpidrs[g_cpu_count++] = affinity;
    }
    return g_cpu_count;
}
int arch_smp_cpu_count(void) { return g_cpu_count; }
uint32_t arch_smp_current_cpu(void) {
    uint32_t id = current_mpidr();
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
    return psci(PSCI_CPU_ON, g_mpidrs[cpu],
                (uint32_t)(uintptr_t)arm_secondary, cpu);
}
void arch_smp_secondary_init(uint32_t cpu) {
    (void)cpu;
    extern void exception_vector(void);
    extern void gic_init(void);
    uintptr_t vector = (uintptr_t)exception_vector;
    __asm__ volatile("mcr p15, 0, %0, c12, c0, 0; isb" :: "r"(vector) : "memory");
    arch_paging_switch(arch_paging_kernel_root());
    gic_init();
}
void arch_smp_send_ipi(uint32_t cpu, uint32_t vector) {
    (void)vector;
    if (cpu >= (uint32_t)g_cpu_count || cpu >= 8) return;
    *(volatile uint32_t*)(uintptr_t)(0x08000000u + 0x0f00u) =
        (1u << (16 + cpu));
}
