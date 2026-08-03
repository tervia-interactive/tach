#include <kernel/types.h>
#include <kernel/klog.h>
#include <hal/irq.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>
#include <kernel/signal.h>

extern uint32_t gic_acknowledge(void);
extern void gic_end_interrupt(uint32_t token);

struct aarch64_exception_frame { uintptr_t x[31]; };

static void frame_to_context(const struct aarch64_exception_frame* frame,
                             struct user_context* context) {
    __asm__ volatile("mrs %0, elr_el1" : "=r"(context->pc));
    __asm__ volatile("mrs %0, sp_el0" : "=r"(context->sp));
    __asm__ volatile("mrs %0, spsr_el1" : "=r"(context->flags));
    for (size_t i = 0; i < 31; i++) context->regs[i] = frame->x[i];
}
static void context_to_frame(const struct user_context* context,
                             struct aarch64_exception_frame* frame) {
    __asm__ volatile("msr elr_el1, %0" :: "r"(context->pc));
    __asm__ volatile("msr sp_el0, %0" :: "r"(context->sp));
    __asm__ volatile("msr spsr_el1, %0" :: "r"(context->flags));
    for (size_t i = 0; i < 31; i++) frame->x[i] = context->regs[i];
}

void aarch64_irq_handler(void) {
    uint32_t token = gic_acknowledge();
    uint32_t irq = token & 0x3ffu;
    if (irq < 1020u) { gic_end_interrupt(token); hal_irq_dispatch((int)irq); }
}

void aarch64_sync_handler(struct aarch64_exception_frame* frame) {
    uint64_t esr, far;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    unsigned exception_class = (unsigned)(esr >> 26);
    struct process* process = process_get_current();

    if (exception_class == 0x15u && process && process->user_mode) {
        frame_to_context(frame, &process->user_context);
        long result = syscall_dispatch((int)frame->x[8], frame->x[0],
            frame->x[1], frame->x[2], frame->x[3], frame->x[4], frame->x[5]);
        process = process_get_current();
        if (!process) return;
        if (process->exec_pending) {
            process->exec_pending = false; process->user_context.regs[0] = 0;
        } else {
            frame_to_context(frame, &process->user_context);
            process->user_context.regs[0] = (uintptr_t)result;
        }
        (void)signal_deliver_pending(process, &process->user_context);
        if (process->state != PROCESS_STATE_RUNNING)
            for (;;) scheduler_yield();
        context_to_frame(&process->user_context, frame);
        return;
    }

    if (exception_class == 0x20u || exception_class == 0x21u ||
        exception_class == 0x24u || exception_class == 0x25u) {
        int write = (esr & (1u << 6)) != 0;
        if (vmm_handle_fault((uintptr_t)far, write) == 0) return;
        if (process && process->user_mode) {
            frame_to_context(frame, &process->user_context);
            process->signal_fault_address = (uintptr_t)far;
            (void)signal_send(process->pid, SIGSEGV);
            (void)signal_deliver_pending(process, &process->user_context);
            if (process->state != PROCESS_STATE_RUNNING)
                for (;;) scheduler_yield();
            context_to_frame(&process->user_context, frame);
            return;
        }
    }
    klog_err("cpu", "AArch64 synchronous exception ESR=%p FAR=%p",
             (void*)(uintptr_t)esr, (void*)(uintptr_t)far);
    for (;;) __asm__ volatile("wfi");
}
