#include <kernel/types.h>
#include <kernel/percpu.h>
#include <hal/paging.h>
#include <hal/smp.h>
#include <hw/acpi.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define LAPIC_BASE 0xfee00000u
#define LAPIC_ID 0x020u
#define LAPIC_EOI 0x0b0u
#define LAPIC_SVR 0x0f0u
#define LAPIC_ICR_LOW 0x300u
#define LAPIC_ICR_HIGH 0x310u
#define LAPIC_LVT_TIMER 0x320u
#define LAPIC_TIMER_INITIAL 0x380u
#define LAPIC_TIMER_DIVIDE 0x3e0u
#define AP_TRAMPOLINE 0x8000u
#define AP_STACK_PAGES 4u

static uint32_t g_apic_ids[MAX_CPUS];
static int g_cpu_count = 1;

extern uint8_t ap_trampoline_start[], ap_trampoline_end[];
extern uint32_t ap_trampoline_cr3, ap_trampoline_cpu;
extern uint64_t ap_trampoline_stack, ap_trampoline_entry;
extern void gdt_init(void);
extern void idt_init(void);

static volatile uint32_t* lapic(void) {
    return (volatile uint32_t*)(uintptr_t)LAPIC_BASE;
}
static uint32_t lapic_read(uint32_t reg) { return lapic()[reg / 4]; }
static void lapic_write(uint32_t reg, uint32_t value) {
    lapic()[reg / 4] = value;
    (void)lapic()[LAPIC_ID / 4];
}
static void short_delay(void) {
    for (volatile uint32_t i = 0; i < 100000u; i++)
        __asm__ volatile("pause");
}
static uint32_t current_apic_id(void) { return lapic_read(LAPIC_ID) >> 24; }
static void enable_lapic(void) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1b));
    low |= 1u << 11;
    __asm__ volatile("wrmsr" :: "a"(low), "d"(high), "c"(0x1b));
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | 0x100u | 0xffu);
}

int arch_smp_init(void) {
    if (vmm_map(vmm_kernel_context(), (void*)(uintptr_t)LAPIC_BASE,
                LAPIC_BASE, VMM_WRITABLE | VMM_NOCACHE) < 0) return 1;
    enable_lapic();
    size_t discovered = acpi_cpu_count();
    uint32_t bsp = current_apic_id();
    g_apic_ids[0] = bsp;
    g_cpu_count = 1;
    for (size_t i = 0; i < discovered && g_cpu_count < MAX_CPUS; i++) {
        uint32_t id = acpi_cpu_apic_id(i);
        if (id != bsp) g_apic_ids[g_cpu_count++] = id;
    }
    return g_cpu_count;
}

int arch_smp_cpu_count(void) { return g_cpu_count; }
uint32_t arch_smp_current_cpu(void) {
    uint32_t id = current_apic_id();
    for (int cpu = 0; cpu < g_cpu_count; cpu++)
        if (g_apic_ids[cpu] == id) return (uint32_t)cpu;
    return 0;
}

int arch_smp_boot_secondary(uint32_t cpu_id, void* entry) {
    if (!cpu_id || cpu_id >= (uint32_t)g_cpu_count || !entry) return -1;
    size_t bytes = (size_t)(ap_trampoline_end - ap_trampoline_start);
    if (bytes > PAGE_SIZE) return -1;
    uint8_t* destination = (uint8_t*)(uintptr_t)AP_TRAMPOLINE;
    for (size_t i = 0; i < bytes; i++) destination[i] = ap_trampoline_start[i];
    void* stack = pmm_alloc_pages(AP_STACK_PAGES);
    if (!stack) return -1;
    *(uint32_t*)(destination + ((uint8_t*)&ap_trampoline_cr3 -
                  ap_trampoline_start)) =
        (uint32_t)(uintptr_t)arch_paging_kernel_root();
    *(uint64_t*)(destination + ((uint8_t*)&ap_trampoline_stack -
                  ap_trampoline_start)) =
        (uint64_t)(uintptr_t)stack + AP_STACK_PAGES * PAGE_SIZE;
    *(uint64_t*)(destination + ((uint8_t*)&ap_trampoline_entry -
                  ap_trampoline_start)) = (uint64_t)(uintptr_t)entry;
    *(uint32_t*)(destination + ((uint8_t*)&ap_trampoline_cpu -
                  ap_trampoline_start)) = cpu_id;

    lapic_write(LAPIC_ICR_HIGH, g_apic_ids[cpu_id] << 24);
    lapic_write(LAPIC_ICR_LOW, 0x0000c500u);
    short_delay();
    lapic_write(LAPIC_ICR_LOW, 0x00008500u);
    short_delay();
    for (int attempt = 0; attempt < 2; attempt++) {
        lapic_write(LAPIC_ICR_HIGH, g_apic_ids[cpu_id] << 24);
        lapic_write(LAPIC_ICR_LOW, 0x00000608u);
        short_delay();
    }
    for (uint32_t wait = 0; wait < 10000000u; wait++) {
        if (hal_smp_cpu_online(cpu_id)) return 0;
        __asm__ volatile("pause");
    }
    return -1;
}

void arch_smp_secondary_init(uint32_t cpu_id) {
    (void)cpu_id;
    enable_lapic();
    gdt_init();
    idt_init();
    arch_paging_switch(arch_paging_kernel_root());
}

void arch_smp_send_ipi(uint32_t cpu_id, uint32_t vector) {
    if (cpu_id >= (uint32_t)g_cpu_count) return;
    lapic_write(LAPIC_ICR_HIGH, g_apic_ids[cpu_id] << 24);
    lapic_write(LAPIC_ICR_LOW, vector & 0xffu);
}

void x86_lapic_timer_init(void) {
    enable_lapic();
    lapic_write(LAPIC_TIMER_DIVIDE, 3u);
    lapic_write(LAPIC_LVT_TIMER, (1u << 17) | 0x20u);
    lapic_write(LAPIC_TIMER_INITIAL, 1000000u);
}
void x86_lapic_eoi(void) {
    if (lapic_read(LAPIC_SVR) & 0x100u) lapic_write(LAPIC_EOI, 0);
}
