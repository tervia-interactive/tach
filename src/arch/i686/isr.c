#include <kernel/klog.h>
#include <kernel/signal.h>
#include <kernel/types.h>
#include <hal/cpu.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>

struct exception_frame32 {
    uint32_t ds;
    uint32_t edi, esi, ebp, saved_esp, ebx, edx, ecx, eax;
    uint32_t vector, error, eip, cs, eflags, useresp, ss;
};

static void to_context(const struct exception_frame32* frame,
                       struct user_context* context) {
    context->pc = frame->eip; context->sp = frame->useresp;
    context->flags = frame->eflags;
    context->regs[0] = frame->eax; context->regs[1] = frame->ebx;
    context->regs[2] = frame->ecx; context->regs[3] = frame->edx;
    context->regs[4] = frame->esi; context->regs[5] = frame->edi;
    context->regs[6] = frame->ebp;
}

static void from_context(const struct user_context* context,
                         struct exception_frame32* frame) {
    frame->eip = context->pc; frame->useresp = context->sp;
    frame->eflags = context->flags | 0x200;
    frame->eax = context->regs[0]; frame->ebx = context->regs[1];
    frame->ecx = context->regs[2]; frame->edx = context->regs[3];
    frame->esi = context->regs[4]; frame->edi = context->regs[5];
    frame->ebp = context->regs[6];
}

void isr_handler(struct exception_frame32* frame) {
    uintptr_t address = 0;
    if (frame->vector == 14) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(address));
        if (vmm_handle_fault(address, (frame->error & 2u) != 0) == 0) return;
    }
    if ((frame->cs & 3u) == 3u) {
        struct process* process = process_get_current();
        if (process) {
            process->signal_fault_address = address;
            to_context(frame, &process->user_context);
            int signal = frame->vector == 0 ? SIGFPE :
                         (frame->vector == 6 ? SIGILL : SIGSEGV);
            (void)signal_send(process->pid, signal);
            (void)signal_deliver_pending(process, &process->user_context);
            if (process->state != PROCESS_STATE_RUNNING)
                for (;;) scheduler_yield();
            from_context(&process->user_context, frame);
            return;
        }
    }
    klog_err("cpu", "i686 kernel exception %u error=0x%x eip=%p",
             (unsigned)frame->vector, (unsigned)frame->error,
             (void*)(uintptr_t)frame->eip);
    hal_cpu_cli();
    for (;;) hal_cpu_halt();
}

void isr_install(void) {}
