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
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4
    movl (0x8000 + ap_trampoline_cr3 - ap_trampoline_start), %eax
    movl %eax, %cr3
    movl $0xc0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr
    movl %cr0, %eax
    orl $0x80000001, %eax
    movl %eax, %cr0
    ljmpl $0x08, $(0x8000 + ap_long_mode - ap_trampoline_start)
.code64
ap_long_mode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movq ap_trampoline_stack(%rip), %rsp
    movl ap_trampoline_cpu(%rip), %edi
    movq ap_trampoline_entry(%rip), %rax
    call *%rax
1:  cli
    hlt
    jmp 1b
.align 8
ap_gdt:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
ap_gdt_pointer:
    .word 23
    .long 0x8000 + ap_gdt - ap_trampoline_start
.align 8
ap_trampoline_cr3: .long 0
ap_trampoline_cpu: .long 0
ap_trampoline_stack: .quad 0
ap_trampoline_entry: .quad 0
ap_trampoline_end:
.section .note.GNU-stack,"",@progbits
