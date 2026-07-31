#include <kernel/string.h>
#include <kernel/types.h>

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

static uint64_t g_gdt[7];
static struct gdt_pointer g_gdt_pointer;
static struct tss64 g_tss;

extern void gdt_flush(uint64_t pointer);

static void install_tss_descriptor(void) {
    uint64_t base = (uint64_t)(uintptr_t)&g_tss;
    uint64_t limit = sizeof(g_tss) - 1;
    g_gdt[5] = (limit & 0xffffu) |
               ((base & 0xffffffu) << 16) |
               (0x89ULL << 40) |
               (((limit >> 16) & 0xfu) << 48) |
               (((base >> 24) & 0xffu) << 56);
    g_gdt[6] = base >> 32;
}

void gdt_init(void) {
    memset(g_gdt, 0, sizeof(g_gdt));
    memset(&g_tss, 0, sizeof(g_tss));
    g_gdt[1] = 0x00af9a000000ffffULL;
    g_gdt[2] = 0x00cf92000000ffffULL;
    g_gdt[3] = 0x00cff2000000ffffULL;
    g_gdt[4] = 0x00affa000000ffffULL;
    g_tss.io_map_base = sizeof(g_tss);
    install_tss_descriptor();
    g_gdt_pointer.limit = sizeof(g_gdt) - 1;
    g_gdt_pointer.base = (uint64_t)(uintptr_t)g_gdt;
    gdt_flush((uint64_t)(uintptr_t)&g_gdt_pointer);
    uint16_t selector = 0x28;
    __asm__ volatile("ltr %0" :: "r"(selector));
}

void arch_user_init(void) {}
bool arch_user_supported(void) { return true; }

void arch_set_kernel_stack(uintptr_t stack_top) {
    g_tss.rsp0 = stack_top;
}
