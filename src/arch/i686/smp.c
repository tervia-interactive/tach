#include <kernel/types.h>
#include <kernel/percpu.h>
#include <hal/paging.h>
#include <hal/smp.h>
#include <hw/acpi.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define LAPIC_BASE 0xfee00000u
#define AP_TRAMPOLINE 0x8000u
#define AP_STACK_PAGES 4u
static uint32_t g_apic_ids[MAX_CPUS];
static int g_cpu_count = 1;
extern uint8_t ap_trampoline_start[], ap_trampoline_end[];
extern uint32_t ap_trampoline_cr3, ap_trampoline_cpu;
extern uint32_t ap_trampoline_stack, ap_trampoline_entry;
extern void gdt_init(void);
extern void idt_init(void);
static volatile uint32_t* lapic(void) {
    return (volatile uint32_t*)(uintptr_t)LAPIC_BASE;
}
static uint32_t read_reg(uint32_t reg) { return lapic()[reg / 4]; }
static void write_reg(uint32_t reg, uint32_t value) {
    lapic()[reg / 4] = value; (void)lapic()[0x20 / 4];
}
static void delay(void) {
    for (volatile uint32_t i = 0; i < 100000u; i++) __asm__ volatile("pause");
}
static void enable_lapic(void) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1b));
    low |= 1u << 11;
    __asm__ volatile("wrmsr" :: "a"(low), "d"(high), "c"(0x1b));
    write_reg(0xf0, read_reg(0xf0) | 0x100u | 0xffu);
}
static uint32_t apic_id(void) { return read_reg(0x20) >> 24; }

int arch_smp_init(void) {
    if (vmm_map(vmm_kernel_context(), (void*)(uintptr_t)LAPIC_BASE,
                LAPIC_BASE, VMM_WRITABLE | VMM_NOCACHE) < 0) return 1;
    enable_lapic();
    uint32_t bsp = apic_id();
    g_apic_ids[0] = bsp; g_cpu_count = 1;
    for (size_t i = 0; i < acpi_cpu_count() && g_cpu_count < MAX_CPUS; i++) {
        uint32_t id = acpi_cpu_apic_id(i);
        if (id != bsp) g_apic_ids[g_cpu_count++] = id;
    }
    return g_cpu_count;
}
int arch_smp_cpu_count(void) { return g_cpu_count; }
uint32_t arch_smp_current_cpu(void) {
    uint32_t id = apic_id();
    for (int cpu = 0; cpu < g_cpu_count; cpu++)
        if (g_apic_ids[cpu] == id) return (uint32_t)cpu;
    return 0;
}
int arch_smp_boot_secondary(uint32_t cpu, void* entry) {
    if (!cpu || cpu >= (uint32_t)g_cpu_count || !entry) return -1;
    size_t bytes = (size_t)(ap_trampoline_end - ap_trampoline_start);
    if (bytes > PAGE_SIZE) return -1;
    uint8_t* out = (uint8_t*)(uintptr_t)AP_TRAMPOLINE;
    for (size_t i = 0; i < bytes; i++) out[i] = ap_trampoline_start[i];
    void* stack = pmm_alloc_pages(AP_STACK_PAGES);
    if (!stack) return -1;
#define PATCH(symbol, type, value) \
    *(type*)(out + ((uint8_t*)&symbol - ap_trampoline_start)) = (type)(value)
    PATCH(ap_trampoline_cr3, uint32_t, arch_paging_kernel_root());
    PATCH(ap_trampoline_stack, uint32_t,
          (uintptr_t)stack + AP_STACK_PAGES * PAGE_SIZE);
    PATCH(ap_trampoline_entry, uint32_t, entry);
    PATCH(ap_trampoline_cpu, uint32_t, cpu);
#undef PATCH
    write_reg(0x310, g_apic_ids[cpu] << 24); write_reg(0x300, 0xc500); delay();
    write_reg(0x300, 0x8500); delay();
    for (int i = 0; i < 2; i++) {
        write_reg(0x310, g_apic_ids[cpu] << 24);
        write_reg(0x300, 0x0608); delay();
    }
    for (uint32_t wait = 0; wait < 10000000u; wait++) {
        if (hal_smp_cpu_online(cpu)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}
void arch_smp_secondary_init(uint32_t cpu) {
    (void)cpu; enable_lapic(); gdt_init(); idt_init();
    arch_paging_switch(arch_paging_kernel_root());
}
void arch_smp_send_ipi(uint32_t cpu, uint32_t vector) {
    if (cpu >= (uint32_t)g_cpu_count) return;
    write_reg(0x310, g_apic_ids[cpu] << 24); write_reg(0x300, vector & 0xffu);
}
void x86_lapic_timer_init(void) {
    enable_lapic(); write_reg(0x3e0, 3); write_reg(0x320, (1u << 17) | 0x20u);
    write_reg(0x380, 1000000u);
}
void x86_lapic_eoi(void) {
    if (read_reg(0xf0) & 0x100u) write_reg(0xb0, 0);
}
