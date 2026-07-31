.section .text
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    lgdt (%rdi)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    pushq $0x08
    lea .Lreload_cs(%rip), %rax
    pushq %rax
    lretq
.Lreload_cs:
    ret
.size gdt_flush, . - gdt_flush
.section .note.GNU-stack,"",@progbits
