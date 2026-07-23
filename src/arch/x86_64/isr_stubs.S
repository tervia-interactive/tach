# x86_64 Exception ISR Stubs
#
# One stub per CPU exception vector (0-31).
#
# Only vectors 8, 10, 11, 12, 13, 14, and 17 get an error code pushed by
# the CPU itself; every other vector gets a dummy 0 pushed here instead,
# so isr_common_stub always sees the same stack shape:
#
#   [rsp+0]  vector      (pushed by us)
#   [rsp+8]  error_code  (pushed by us as 0, or by the CPU)
#   [rsp+16] rip         - all three pushed by the CPU on any
#   [rsp+24] cs            interrupt/exception, in that order
#   [rsp+32] rflags
#
# No RSP/SS here: those only get pushed by the CPU on a privilege-level
# change, and every one of these gates runs at CPL0 while the kernel
# itself is still CPL0, so there is no ring transition.
#
# This intentionally never returns (no IRETQ): isr_handler() in isr.c
# logs what happened and halts. Getting here at all means something is
# broken badly enough that resuming isn't meaningful yet.

.section .text
.code64

.macro ISR_NOERR num
.global isr\num
isr\num:
    push $0
    push $\num
    jmp isr_common_stub
.endm

.macro ISR_ERR num
.global isr\num
isr\num:
    push $\num
    jmp isr_common_stub
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

isr_common_stub:
    mov %rsp, %rdi
    call isr_handler
.hang:
    cli
    hlt
    jmp .hang

.section .data
.align 8
.global isr_stub_table
isr_stub_table:
    .quad isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
    .quad isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    .quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
