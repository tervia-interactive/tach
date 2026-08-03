.section .text
.global idt_flush
.type idt_flush, @function
idt_flush:
    lidt (%rdi)
    ret
.size idt_flush, . - idt_flush
.section .note.GNU-stack,"",@progbits
