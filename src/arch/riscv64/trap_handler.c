/* RISC-V 64-bit Trap Handler (C side, called from trap_vector in traps.S) */
#include <kernel/types.h>

void riscv_trap_handler(uintptr_t cause, uintptr_t epc) {
    (void)cause;
    (void)epc;
}
