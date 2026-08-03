#include <kernel/types.h>
#include <kernel/klog.h>
#include <hal/irq.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <kernel/signal.h>

extern uint32_t gic_acknowledge(void);
extern void gic_end_interrupt(uint32_t token);

void aarch64_irq_handler(void) {
    uint32_t token = gic_acknowledge();
    uint32_t irq = token & 0x3ffu;
    if (irq < 1020u) {
        gic_end_interrupt(token);
        hal_irq_dispatch((int)irq);
    }
}

void aarch64_sync_handler(void) {
    uint64_t esr, far;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    unsigned exception_class = (unsigned)(esr >> 26);
    if (exception_class == 0x24u || exception_class == 0x25u) {
        int write = (esr & (1u << 6)) != 0;
        if (vmm_handle_fault((uintptr_t)far, write) == 0) return;
        struct process* process = process_get_current();
        if (process && process->user_mode) {
            process->signal_fault_address = (uintptr_t)far;
            (void)signal_send(process->pid, SIGSEGV);
            return;
        }
    }
    klog_err("cpu", "AArch64 synchronous exception ESR=%p FAR=%p",
             (void*)(uintptr_t)esr, (void*)(uintptr_t)far);
    for (;;) __asm__ volatile("wfi");
}
