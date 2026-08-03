.section .text
.global _start
.type _start, %function
_start:
    mov x29, xzr
    ldr x0, [sp]
    add x1, sp, #8
    bl main
    bl _exit
.size _start, . - _start
.section .note.GNU-stack,"",%progbits
