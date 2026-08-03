#include <kernel/signal.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>

struct int80_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
};

static void frame_to_context(const struct int80_frame* frame,
                             struct user_context* context) {
    context->pc = frame->rip;
    context->sp = frame->rsp;
    context->flags = frame->rflags;
    context->regs[0] = frame->rax;
    context->regs[1] = frame->rdi;
    context->regs[2] = frame->rsi;
    context->regs[3] = frame->rdx;
    context->regs[4] = frame->r10;
    context->regs[5] = frame->r8;
    context->regs[6] = frame->r9;
    context->regs[7] = frame->rbx;
    context->regs[8] = frame->rbp;
    context->regs[9] = frame->rcx;
    context->regs[10] = frame->r11;
    context->regs[11] = frame->r12;
    context->regs[12] = frame->r13;
    context->regs[13] = frame->r14;
    context->regs[14] = frame->r15;
}

static void context_to_frame(const struct user_context* context,
                             struct int80_frame* frame) {
    frame->rip = context->pc;
    frame->rsp = context->sp;
    frame->rflags = context->flags | 0x200;
    frame->rax = context->regs[0];
    frame->rdi = context->regs[1];
    frame->rsi = context->regs[2];
    frame->rdx = context->regs[3];
    frame->r10 = context->regs[4];
    frame->r8 = context->regs[5];
    frame->r9 = context->regs[6];
    frame->rbx = context->regs[7];
    frame->rbp = context->regs[8];
    frame->rcx = context->regs[9];
    frame->r11 = context->regs[10];
    frame->r12 = context->regs[11];
    frame->r13 = context->regs[12];
    frame->r14 = context->regs[13];
    frame->r15 = context->regs[14];
}

void x86_64_syscall_entry(struct int80_frame* frame) {
    struct process* process = process_get_current();
    if (!process || !frame) return;
    frame_to_context(frame, &process->user_context);
    long result = syscall_dispatch((int)frame->rax, frame->rdi, frame->rsi,
        frame->rdx, frame->r10, frame->r8, frame->r9);
    process = process_get_current();
    if (!process) return;
    if (process->exec_pending) {
        process->exec_pending = false;
        process->user_context.regs[0] = 0;
    } else {
        frame_to_context(frame, &process->user_context);
        process->user_context.regs[0] = (uintptr_t)result;
    }
    (void)signal_deliver_pending(process, &process->user_context);
    if (process->state != PROCESS_STATE_RUNNING) {
        for (;;) scheduler_yield();
    }
    context_to_frame(&process->user_context, frame);
}
