#include <kernel/types.h>

/*
 * Long-mode IDT gates are 16 bytes, not the 8-byte 32-bit-style gate this
 * file used to define (that format silently produced a garbage IDT on
 * x86_64 - every "entry" actually straddled two real gates). This is the
 * correct x86_64 layout:
 *   bytes 0-1  : base 0:15
 *   bytes 2-3  : segment selector
 *   byte  4    : IST (interrupt stack table index, 0 = none)
 *   byte  5    : type/attributes (present, DPL, gate type)
 *   bytes 6-7  : base 16:31
 *   bytes 8-11 : base 32:63
 *   bytes 12-15: reserved, must be zero
 */
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* 32-bit CPU code selector set up by boot.S's temporary GDT (gdt64_code,
 * the second entry -> offset 0x08). */
#define KERNEL_CODE_SELECTOR 0x08

/* Present | DPL=0 | 64-bit interrupt gate (type 0xE) */
#define IDT_FLAGS_INT_GATE 0x8E

static struct idt_entry idt[256];
static struct idt_ptr idtp;

/* Exception and remapped legacy PIC entry points. */
extern uint64_t isr_stub_table[32];
extern uint64_t irq_stub_table[16];
extern void int80_stub(void);
extern void idt_flush(uint64_t);

void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low  = (uint16_t)(base & 0xFFFF);
    idt[num].base_mid  = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].base_high = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    idt[num].selector  = selector;
    idt[num].ist       = 0;
    idt[num].flags     = flags;
    idt[num].reserved  = 0;
}

void idt_init(void) {
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, 0, 0, 0); /* not present */
    }

    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], KERNEL_CODE_SELECTOR, IDT_FLAGS_INT_GATE);
    }
    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(32 + i), irq_stub_table[i],
                     KERNEL_CODE_SELECTOR, IDT_FLAGS_INT_GATE);
    }
    idt_set_gate(0x80, (uint64_t)(uintptr_t)int80_stub,
                 KERNEL_CODE_SELECTOR, 0xee);

    idt_flush((uint64_t)&idtp);
}
