#include <kernel/types.h>
#include <kernel/klog.h>
#include <hal/cpu.h>

/*
 * Matches the stack layout isr_stubs.S builds (see the comment there):
 * no rsp/ss fields, because every one of these gates runs at CPL0 with
 * no privilege-level change, so the CPU doesn't push them.
 */
typedef struct {
    uint64_t vector;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} isr_frame_t;

static const char* exception_name(uint64_t vector) {
    switch (vector) {
        case 0:  return "Divide Error (#DE)";
        case 1:  return "Debug (#DB)";
        case 2:  return "Non-Maskable Interrupt";
        case 3:  return "Breakpoint (#BP)";
        case 4:  return "Overflow (#OF)";
        case 5:  return "BOUND Range Exceeded (#BR)";
        case 6:  return "Invalid Opcode (#UD)";
        case 7:  return "Device Not Available (#NM) - FPU/SSE state not set up";
        case 8:  return "Double Fault (#DF)";
        case 9:  return "Coprocessor Segment Overrun";
        case 10: return "Invalid TSS (#TS)";
        case 11: return "Segment Not Present (#NP)";
        case 12: return "Stack-Segment Fault (#SS)";
        case 13: return "General Protection Fault (#GP)";
        case 14: return "Page Fault (#PF)";
        case 16: return "x87 Floating-Point Error (#MF)";
        case 17: return "Alignment Check (#AC)";
        case 18: return "Machine Check (#MC)";
        case 19: return "SIMD Floating-Point Exception (#XM)";
        case 20: return "Virtualization Exception (#VE)";
        default: return "Reserved/unknown exception";
    }
}

/* Called by isr_common_stub in isr_stubs.S. There is no IRETQ path back:
 * hitting any of these 32 vectors means something is broken badly enough
 * that this early in boot, resuming isn't meaningful. Logging exactly
 * what happened and halting is strictly more useful than the silent
 * reset a triple fault would otherwise produce (no IDT == any fault
 * here escalates to a double fault, then a triple fault, with nothing
 * ever printed). */
void isr_handler(isr_frame_t* regs) {
    klog_err("cpu", "unhandled exception %u: %s", (unsigned)regs->vector,
             exception_name(regs->vector));
    klog_err("cpu", "error_code=0x%x rip=%p cs=0x%x rflags=%p",
             (unsigned)regs->err_code, (void*)regs->rip,
             (unsigned)regs->cs, (void*)regs->rflags);

    if (regs->vector == 14) {
        uint64_t fault_addr;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
        klog_err("cpu", "page fault accessing address %p", (void*)fault_addr);
    }

    klog_err("cpu", "system halted - this is where execution stopped");

    hal_cpu_cli();
    for (;;) {
        hal_cpu_halt();
    }
}

/* idt_init() (arch/x86_64/idt.c) installs isr_stub_table[] directly, so
 * there's nothing left for this to do. Kept so existing callers/headers
 * that reference it still link. */
void isr_install(void) {
}
