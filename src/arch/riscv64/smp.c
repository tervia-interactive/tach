#include <boot/sbi.h>
#include <hal/paging.h>
#include <hal/smp.h>
#include <kernel/percpu.h>
#include <mm/pmm.h>

static uintptr_t g_harts[MAX_CPUS];
static int g_hart_count = 1;
static uintptr_t g_secondary_stacks[MAX_CPUS] __attribute__((used));
static uintptr_t current_hart(void) {
    uintptr_t hart; __asm__ volatile("mv %0, tp" : "=r"(hart)); return hart;
}
__attribute__((naked)) static void riscv_secondary(
    uintptr_t hart __attribute__((unused)),
    uintptr_t logical_cpu __attribute__((unused))) {
    __asm__ volatile(
        "mv tp, a0\n"
        "la t0, g_secondary_stacks\n"
        "slli t1, a1, 3\n"
        "add t0, t0, t1\n"
        "ld sp, 0(t0)\n"
        "mv a0, a1\n"
        "tail hal_smp_secondary_entry\n");
}
int arch_smp_init(void) {
    uintptr_t boot = current_hart(); g_harts[0] = boot; g_hart_count = 1;
    for (uintptr_t hart = 0; hart < 64 && g_hart_count < MAX_CPUS; hart++) {
        if (hart == boot) continue;
        struct sbi_return status = sbi_ecall(SBI_EXT_ID_HSM,
            SBI_FID_HSM_HART_GET_STATUS, (long)hart, 0, 0, 0, 0, 0);
        if (!status.error) g_harts[g_hart_count++] = hart;
    }
    return g_hart_count;
}
int arch_smp_cpu_count(void) { return g_hart_count; }
uint32_t arch_smp_current_cpu(void) {
    uintptr_t hart = current_hart();
    for (int cpu = 0; cpu < g_hart_count; cpu++)
        if (g_harts[cpu] == hart) return (uint32_t)cpu;
    return 0;
}
int arch_smp_boot_secondary(uint32_t cpu, void* entry) {
    (void)entry;
    if (!cpu || cpu >= (uint32_t)g_hart_count) return -1;
    void* stack = pmm_alloc_pages(4);
    if (!stack) return -1;
    g_secondary_stacks[cpu] = (uintptr_t)stack + 4 * PAGE_SIZE;
    struct sbi_return result = sbi_ecall(SBI_EXT_ID_HSM,
        SBI_FID_HSM_HART_START, (long)g_harts[cpu],
        (long)(uintptr_t)riscv_secondary, (long)cpu, 0, 0, 0);
    return (int)result.error;
}
void arch_smp_secondary_init(uint32_t cpu) {
    (void)cpu;
    extern void trap_vector(void);
    uintptr_t vector = (uintptr_t)trap_vector;
    __asm__ volatile("csrw stvec, %0" :: "r"(vector) : "memory");
    arch_paging_switch(arch_paging_kernel_root());
}
void arch_smp_send_ipi(uint32_t cpu, uint32_t vector) {
    (void)vector;
    if (cpu >= (uint32_t)g_hart_count) return;
    uintptr_t base = g_harts[cpu] & ~(uintptr_t)(sizeof(uintptr_t) * 8 - 1);
    uintptr_t mask = (uintptr_t)1 << (g_harts[cpu] - base);
    (void)sbi_ecall(SBI_EXT_ID_IPI, SBI_FID_IPI_SEND_IPI,
                    (long)mask, (long)base, 0, 0, 0, 0);
}
