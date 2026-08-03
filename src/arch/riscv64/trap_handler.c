#include <kernel/types.h>
#include <hal/irq.h>
#include <kernel/signal.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>

struct riscv_trap_frame { uintptr_t x[32], epc, status; };

static void to_context(const struct riscv_trap_frame* frame,
                       struct user_context* context) {
    context->pc = frame->epc; context->sp = frame->x[2];
    context->flags = frame->status;
    for (size_t i = 0; i < 32; i++) context->regs[i] = frame->x[i];
}
static void from_context(const struct user_context* context,
                         struct riscv_trap_frame* frame) {
    for (size_t i = 0; i < 32; i++) frame->x[i] = context->regs[i];
    frame->epc = context->pc; frame->x[2] = context->sp;
    frame->status = (context->flags & ~(uintptr_t)0x100u) | 0x20u;
}

void riscv_trap_handler(struct riscv_trap_frame* frame) {
    uintptr_t cause, value;
    __asm__ volatile("csrr %0, scause" : "=r"(cause));
    __asm__ volatile("csrr %0, stval" : "=r"(value));
    const uintptr_t interrupt = (uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1);
    uintptr_t code = cause & ~interrupt;
    if (cause & interrupt) { if (code == 5) hal_irq_dispatch(5); return; }
    struct process* process = process_get_current();
    bool from_user = !(frame->status & 0x100u);
    if (code == 8 && from_user && process && process->user_mode) {
        frame->epc += 4; to_context(frame, &process->user_context);
        long result = syscall_dispatch((int)frame->x[17], frame->x[10],
            frame->x[11], frame->x[12], frame->x[13], frame->x[14], frame->x[15]);
        process = process_get_current(); if (!process) return;
        if (process->exec_pending) {
            process->exec_pending = false; process->user_context.regs[10] = 0;
        } else {
            to_context(frame, &process->user_context);
            process->user_context.regs[10] = (uintptr_t)result;
        }
        (void)signal_deliver_pending(process, &process->user_context);
        if (process->state != PROCESS_STATE_RUNNING)
            for (;;) scheduler_yield();
        from_context(&process->user_context, frame); return;
    }
    if (code == 12 || code == 13 || code == 15) {
        if (vmm_handle_fault(value, code == 15) == 0) return;
        if (from_user && process && process->user_mode) {
            to_context(frame, &process->user_context);
            process->signal_fault_address = value;
            (void)signal_send(process->pid, SIGSEGV);
            (void)signal_deliver_pending(process, &process->user_context);
            if (process->state != PROCESS_STATE_RUNNING)
                for (;;) scheduler_yield();
            from_context(&process->user_context, frame);
        }
    }
}
