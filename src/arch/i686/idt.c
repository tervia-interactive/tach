#include <kernel/types.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idt_ptr;

extern void idt_flush(uint32_t);
extern uint32_t isr_stub_table[32];
extern uint32_t irq_stub_table[16];
extern void int80_stub(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

void idt_init(void) {
    idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_ptr.base = (uint32_t)&idt;
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    for (int i = 0; i < 32; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], 0x08, 0x8e);
    }
    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(32 + i), irq_stub_table[i], 0x08, 0x8e);
    }
    idt_set_gate(0x80, (uint32_t)(uintptr_t)int80_stub, 0x08, 0xee);
    idt_flush((uint32_t)&idt_ptr);
}
