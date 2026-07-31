#include <kernel/types.h>
#include <hal/irq.h>
#include <kernel/signal.h>
#include <mm/vmm.h>
#include <proc/process.h>

void riscv_trap_handler(uintptr_t cause, uintptr_t epc) {
    uintptr_t code = cause & 0x7fffffffu;
    if (cause & 0x80000000u) {
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
