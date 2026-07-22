#include <kernel/types.h>
void irq_install(void) { }
void irq_enable(void) { __asm__ volatile ("sti"); }
void irq_disable(void) { __asm__ volatile ("cli"); }
