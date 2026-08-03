#include <kernel/types.h>
#include <hal/irq.h>
#include <kernel/signal.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>

extern uint32_t gic_acknowledge(void);
extern void gic_end_interrupt(uint32_t token);

struct arm_user_frame {
    uint32_t r[13], user_sp, user_lr, return_pc, spsr;
};

static void frame_to_context(const struct arm_user_frame* frame,
                             struct user_context* context) {
    context->pc = frame->return_pc; context->sp = frame->user_sp;
    context->flags = frame->spsr;
    for (size_t i = 0; i < 13; i++) context->regs[i] = frame->r[i];
    context->regs[14] = frame->user_lr;
}
static void context_to_frame(const struct user_context* context,
                             struct arm_user_frame* frame) {
    frame->return_pc = (uint32_t)context->pc;
    frame->user_sp = (uint32_t)context->sp;
    frame->user_lr = (uint32_t)context->regs[14];
    frame->spsr = ((uint32_t)context->flags & ~0x1fu) | 0x10u;
    for (size_t i = 0; i < 13; i++) frame->r[i] = (uint32_t)context->regs[i];
}

void arm_irq_handler(void) {
    uint32_t token = gic_acknowledge();
    uint32_t irq = token & 0x3ffu;
    if (irq < 1020u) { gic_end_interrupt(token); hal_irq_dispatch((int)irq); }
}

void arm_syscall_handler(struct arm_user_frame* frame) {
    struct process* process = process_get_current();
    if (!process || !frame) return;
    frame_to_context(frame, &process->user_context);
    long result = syscall_dispatch((int)frame->r[7], frame->r[0], frame->r[1],
        frame->r[2], frame->r[3], frame->r[4], frame->r[5]);
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
}

void arm_data_abort_handler(void) {
    uint32_t address, status;
    __asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(address));
    __asm__ volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(status));
    if (vmm_handle_fault(address, (status & (1u << 11)) != 0) == 0) return;
    struct process* process = process_get_current();
    if (process && process->user_mode) {
        process->signal_fault_address = address;
        (void)signal_send(process->pid, SIGSEGV);
        (void)signal_deliver_pending(process, &process->user_context);
        if (process->state != PROCESS_STATE_RUNNING)
            for (;;) scheduler_yield();
    }
}
