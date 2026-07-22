#include <kernel/types.h>
typedef struct { uint64_t int_no, err_code, rip, cs, rflags, rsp, ss; } registers_t;
void isr_handler(registers_t *regs) { (void)regs; }
void isr_install(void) { }
