#include <kernel/types.h>
struct idt_entry { uint16_t base_low; uint16_t selector; uint8_t zero; uint8_t flags; uint16_t base_high; } __attribute__((packed));
struct idt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
static struct idt_entry idt[256]; static struct idt_ptr idt_ptr;
extern void idt_flush(uint64_t);
void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF); idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector; idt[num].zero = 0; idt[num].flags = flags;
}
void idt_init(void) { idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1; idt_ptr.base = (uint64_t)&idt; }
