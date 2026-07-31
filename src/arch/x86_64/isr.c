#include <kernel/types.h>
#include <kernel/klog.h>
#include <kernel/signal.h>
#include <hal/cpu.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>

struct exception_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags, rsp, ss;
};

static const char* exception_name(uint64_t vector) {
    static const char* names[21] = {
        "divide error", "debug", "NMI", "breakpoint", "overflow",
        "bound range", "invalid opcode", "device unavailable",
        "double fault", "coprocessor overrun", "invalid TSS",
        "segment missing", "stack fault", "general protection",
        "page fault", "reserved", "x87 error", "alignment check",
        "machine check", "SIMD error", "virtualization"
    };
    return vector < 21 ? names[vector] : "reserved exception";
}

static int exception_signal(uint64_t vector) {
    if (vector == 0 || vector == 16 || vector == 19) return SIGFPE;
    if (vector == 6) return SIGILL;
    if (vector == 14) return SIGSEGV;
    return SIGSEGV;
}

static void frame_to_context(const struct exception_frame* frame,
                             struct user_context* context) {
    context->pc = frame->rip; context->sp = frame->rsp;
    context->flags = frame->rflags;
    context->regs[0] = frame->rax; context->regs[1] = frame->rdi;
    context->regs[2] = frame->rsi; context->regs[3] = frame->rdx;
    context->regs[4] = frame->r10; context->regs[5] = frame->r8;
    context->regs[6] = frame->r9; context->regs[7] = frame->rbx;
    context->regs[8] = frame->rbp; context->regs[9] = frame->rcx;
    context->regs[10] = frame->r11; context->regs[11] = frame->r12;
    context->regs[12] = frame->r13; context->regs[13] = frame->r14;
    context->regs[14] = frame->r15;
}

static void context_to_frame(const struct user_context* context,
                             struct exception_frame* frame) {
    frame->rip = context->pc; frame->rsp = context->sp;
    frame->rflags = context->flags | 0x200;
    frame->rax = context->regs[0]; frame->rdi = context->regs[1];
    frame->rsi = context->regs[2]; frame->rdx = context->regs[3];
    frame->r10 = context->regs[4]; frame->r8 = context->regs[5];
    frame->r9 = context->regs[6]; frame->rbx = context->regs[7];
    frame->rbp = context->regs[8]; frame->rcx = context->regs[9];
    frame->r11 = context->regs[10]; frame->r12 = context->regs[11];
    frame->r13 = context->regs[12]; frame->r14 = context->regs[13];
    frame->r15 = context->regs[14];
}

void isr_handler(struct exception_frame* frame) {
    bool from_user = (frame->cs & 3u) == 3u;
    uintptr_t fault_address = 0;
    if (frame->vector == 14) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_address));
        if (vmm_handle_fault(fault_address,
                (frame->error & (1u << 1)) != 0) == 0) return;
    }
    if (from_user) {
        struct process* process = process_get_current();
        if (process) {
            process->signal_fault_address = fault_address;
            frame_to_context(frame, &process->user_context);
            (void)signal_send(process->pid, exception_signal(frame->vector));
            (void)signal_deliver_pending(process, &process->user_context);
            if (process->state != PROCESS_STATE_RUNNING)
                for (;;) scheduler_yield();
            context_to_frame(&process->user_context, frame);
            return;
        }
    }
    klog_err("cpu", "kernel exception %u (%s), error=0x%x rip=%p",
             (unsigned)frame->vector, exception_name(frame->vector),
             (unsigned)frame->error, (void*)frame->rip);
    if (frame->vector == 14)
        klog_err("cpu", "page fault address=%p", (void*)fault_address);
    hal_cpu_cli();
    for (;;) hal_cpu_halt();
}

void isr_install(void) {}
