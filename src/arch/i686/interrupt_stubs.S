.section .text
.code32

.macro ISR_NOERR number
.global isr\number
isr\number:
    pushl $0
    pushl $\number
    jmp isr_common_stub
.endm
.macro ISR_ERR number
.global isr\number
isr\number:
    pushl $\number
    jmp isr_common_stub
.endm
.macro IRQ_STUB number
.global irq\number
irq\number:
    pushl $0
    pushl $(32 + \number)
    jmp irq_common_stub
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17
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
ISR_ERR 30
ISR_NOERR 31

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

.macro SAVE_CONTEXT
    pusha
    xorl %eax, %eax
    movw %ds, %ax
    pushl %eax
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
.endm
.macro RESTORE_CONTEXT
    popl %eax
    movw %ax, %ds
    movw %ax, %es
    popa
    addl $8, %esp
    iret
.endm

isr_common_stub:
    SAVE_CONTEXT
    pushl %esp
    call isr_handler
    addl $4, %esp
    RESTORE_CONTEXT

irq_common_stub:
    SAVE_CONTEXT
    movl 36(%esp), %eax
    subl $32, %eax
    pushl %eax
    call x86_irq_handler
    addl $4, %esp
    RESTORE_CONTEXT

.global int80_stub
int80_stub:
    pusha
    pushl %esp
    call i686_syscall_entry
    addl $4, %esp
    popa
    iret

.section .data
.align 4
.global isr_stub_table
isr_stub_table:
    .long isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .long isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
.global irq_stub_table
irq_stub_table:
    .long irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
    .long irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

.section .note.GNU-stack,"",@progbits
