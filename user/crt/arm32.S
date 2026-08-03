.section .text
.global _start
.type _start, %function
_start:
    mov r11, #0
    ldr r0, [sp]
    add r1, sp, #4
    bl main
    bl _exit
.size _start, . - _start
.section .note.GNU-stack,"",%progbits
