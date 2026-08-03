.section .text
.global _start
_start:
    xorq %rbp, %rbp
    movq (%rsp), %rdi
    leaq 8(%rsp), %rsi
    call main
    movl %eax, %edi
    call _exit
.section .note.GNU-stack,"",@progbits
