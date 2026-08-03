#include <kernel/signal.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>

struct int80_frame32 {
    uint32_t edi, esi, ebp, saved_esp, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags, useresp, ss;
};

static void to_context(const struct int80_frame32* frame,
                       struct user_context* context) {
    context->pc = frame->eip; context->sp = frame->useresp;
    context->flags = frame->eflags;
    context->regs[0] = frame->eax; context->regs[1] = frame->ebx;
    context->regs[2] = frame->ecx; context->regs[3] = frame->edx;
    context->regs[4] = frame->esi; context->regs[5] = frame->edi;
    context->regs[6] = frame->ebp;
}
static void from_context(const struct user_context* context,
                         struct int80_frame32* frame) {
    frame->eip = context->pc; frame->useresp = context->sp;
    frame->eflags = context->flags | 0x200;
    frame->eax = context->regs[0]; frame->ebx = context->regs[1];
    frame->ecx = context->regs[2]; frame->edx = context->regs[3];
    frame->esi = context->regs[4]; frame->edi = context->regs[5];
    frame->ebp = context->regs[6];
}

void i686_syscall_entry(struct int80_frame32* frame) {
    struct process* process = process_get_current();
    if (!process || !frame) return;
    to_context(frame, &process->user_context);
    long result = syscall_dispatch((int)frame->eax, frame->ebx, frame->ecx,
        frame->edx, frame->esi, frame->edi, frame->ebp);
    process = process_get_current();
    if (!process) return;
    if (process->exec_pending) {
        process->exec_pending = false;
        process->user_context.regs[0] = 0;
    } else {
        to_context(frame, &process->user_context);
        process->user_context.regs[0] = (uintptr_t)result;
    }
    (void)signal_deliver_pending(process, &process->user_context);
    if (process->state != PROCESS_STATE_RUNNING)
        for (;;) scheduler_yield();
    from_context(&process->user_context, frame);
}
