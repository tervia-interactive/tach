#include <kernel/string.h>
#include <kernel/types.h>
#include <kernel/percpu.h>
#include <hal/smp.h>

struct gdt_pointer { uint16_t limit; uint64_t base; } __attribute__((packed));
struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
} __attribute__((packed));

static uint64_t g_gdt[MAX_CPUS][7];
static struct gdt_pointer g_gdt_pointer[MAX_CPUS];
static struct tss64 g_tss[MAX_CPUS];

extern void gdt_flush(uint64_t pointer);

static void install_tss_descriptor(uint32_t cpu) {
    uint64_t base = (uint64_t)(uintptr_t)&g_tss[cpu];
    uint64_t limit = sizeof(g_tss[cpu]) - 1;
    g_gdt[cpu][5] = (limit & 0xffffu) |
               ((base & 0xffffffu) << 16) |
               (0x89ULL << 40) |
               (((limit >> 16) & 0xfu) << 48) |
               (((base >> 24) & 0xffu) << 56);
    g_gdt[cpu][6] = base >> 32;
}

void gdt_init(void) {
    uint32_t cpu = hal_smp_current_cpu();
    memset(g_gdt[cpu], 0, sizeof(g_gdt[cpu]));
    memset(&g_tss[cpu], 0, sizeof(g_tss[cpu]));
    g_gdt[cpu][1] = 0x00af9a000000ffffULL;
    g_gdt[cpu][2] = 0x00cf92000000ffffULL;
    g_gdt[cpu][3] = 0x00cff2000000ffffULL;
    g_gdt[cpu][4] = 0x00affa000000ffffULL;
    g_tss[cpu].io_map_base = sizeof(g_tss[cpu]);
    install_tss_descriptor(cpu);
    g_gdt_pointer[cpu].limit = sizeof(g_gdt[cpu]) - 1;
    g_gdt_pointer[cpu].base = (uint64_t)(uintptr_t)g_gdt[cpu];
    gdt_flush((uint64_t)(uintptr_t)&g_gdt_pointer[cpu]);
    uint16_t selector = 0x28;
    __asm__ volatile("ltr %0" :: "r"(selector));
}

void arch_user_init(void) {}
bool arch_user_supported(void) { return true; }

void arch_set_kernel_stack(uintptr_t stack_top) {
    g_tss[hal_smp_current_cpu()].rsp0 = stack_top;
}
