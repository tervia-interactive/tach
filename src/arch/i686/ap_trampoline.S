.section .text
.code16
.global ap_trampoline_start, ap_trampoline_end
.global ap_trampoline_cr3, ap_trampoline_stack
.global ap_trampoline_entry, ap_trampoline_cpu
ap_trampoline_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    lgdtw (0x8000 + ap_gdt_pointer - ap_trampoline_start)
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $(0x8000 + ap_protected - ap_trampoline_start)
.code32
ap_protected:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl (0x8000 + ap_trampoline_cr3 - ap_trampoline_start), %eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    movl (0x8000 + ap_trampoline_stack - ap_trampoline_start), %esp
    pushl (0x8000 + ap_trampoline_cpu - ap_trampoline_start)
    movl (0x8000 + ap_trampoline_entry - ap_trampoline_start), %eax
    call *%eax
1:  cli
    hlt
    jmp 1b
.align 8
ap_gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
ap_gdt_pointer:
    .word 23
    .long 0x8000 + ap_gdt - ap_trampoline_start
.align 4
ap_trampoline_cr3: .long 0
ap_trampoline_cpu: .long 0
ap_trampoline_stack: .long 0
ap_trampoline_entry: .long 0
ap_trampoline_end:
.section .note.GNU-stack,"",@progbits
