#include <kernel/string.h>
#include <kernel/types.h>
#include <kernel/percpu.h>
#include <hal/smp.h>

struct gdt_entry {
    uint16_t limit_low, base_low;
    uint8_t base_middle, access, granularity, base_high;
} __attribute__((packed));
struct gdt_pointer { uint16_t limit; uint32_t base; } __attribute__((packed));
struct tss32 {
    uint32_t previous, esp0, ss0, esp1, ss1, esp2, ss2, cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, io_map_base;
} __attribute__((packed));

static struct gdt_entry g_gdt[MAX_CPUS][6];
static struct gdt_pointer g_pointer[MAX_CPUS];
static struct tss32 g_tss[MAX_CPUS];
extern void gdt_flush(uint32_t);

static void set_gate(uint32_t cpu, int index, uint32_t base, uint32_t limit,
                     uint8_t access, uint8_t granularity) {
    g_gdt[cpu][index].base_low = base & 0xffffu;
    g_gdt[cpu][index].base_middle = (base >> 16) & 0xffu;
    g_gdt[cpu][index].base_high = (base >> 24) & 0xffu;
    g_gdt[cpu][index].limit_low = limit & 0xffffu;
    g_gdt[cpu][index].granularity = ((limit >> 16) & 0x0fu) |
                               (granularity & 0xf0u);
    g_gdt[cpu][index].access = access;
}

void gdt_init(void) {
    uint32_t cpu = hal_smp_current_cpu();
    memset(g_gdt[cpu], 0, sizeof(g_gdt[cpu]));
    memset(&g_tss[cpu], 0, sizeof(g_tss[cpu]));
    set_gate(cpu, 1, 0, 0xffffffffu, 0x9a, 0xcf);
    set_gate(cpu, 2, 0, 0xffffffffu, 0x92, 0xcf);
    set_gate(cpu, 3, 0, 0xffffffffu, 0xfa, 0xcf);
    set_gate(cpu, 4, 0, 0xffffffffu, 0xf2, 0xcf);
    g_tss[cpu].ss0 = 0x10;
    g_tss[cpu].io_map_base = sizeof(g_tss[cpu]);
    set_gate(cpu, 5, (uint32_t)(uintptr_t)&g_tss[cpu],
             sizeof(g_tss[cpu]) - 1, 0x89, 0x00);
    g_pointer[cpu].limit = sizeof(g_gdt[cpu]) - 1;
    g_pointer[cpu].base = (uint32_t)(uintptr_t)g_gdt[cpu];
    gdt_flush((uint32_t)(uintptr_t)&g_pointer[cpu]);
    uint16_t selector = 0x28;
    __asm__ volatile("ltr %0" :: "r"(selector));
}

void arch_user_init(void) {}
bool arch_user_supported(void) { return true; }
void arch_set_kernel_stack(uintptr_t stack_top) {
    g_tss[hal_smp_current_cpu()].esp0 = (uint32_t)stack_top;
}
