.section .text
.global _start
.type _start, @function
_start:
    li s0, 0
    lw a0, 0(sp)
    addi a1, sp, 4
    call main
    call _exit
.size _start, . - _start
.section .note.GNU-stack,"",@progbits
