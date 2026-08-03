.section .text
.code32
.global _start
_start:
    xorl %ebp, %ebp
    movl (%esp), %eax
    leal 4(%esp), %edx
    pushl %edx
    pushl %eax
    call main
    addl $8, %esp
    pushl %eax
    call _exit
.section .note.GNU-stack,"",@progbits
