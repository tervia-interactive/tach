#include <kernel/types.h>
#include <hal/irq.h>
#include <kernel/signal.h>
#include <mm/vmm.h>
#include <proc/process.h>

void riscv_trap_handler(uintptr_t cause, uintptr_t epc) {
    const uintptr_t interrupt = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
    uintptr_t code = cause & ~interrupt;
    if (cause & interrupt) {
        if (code == 5) hal_irq_dispatch(5);
        return;
    }
    if (code == 12 || code == 13 || code == 15) {
        uintptr_t address;
        __asm__ volatile("csrr %0, stval" : "=r"(address));
        if (vmm_handle_fault(address, code == 15) == 0) return;
        struct process* process = process_get_current();
        if (process && process->user_mode) {
            process->signal_fault_address = address;
            (void)signal_send(process->pid, SIGSEGV);
        }
    }
    (void)epc;
}
