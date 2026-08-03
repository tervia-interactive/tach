.section .text
.global arm32_enter_user
.type arm32_enter_user, %function
arm32_enter_user:
    mov r12, r0
    cps #0x1f
    ldr sp, [r12, #4]
    ldr lr, [r12, #68]
    cps #0x13
    ldr lr, [r12, #0]
    mov r1, #0x10
    msr spsr_cxsf, r1
    ldr r0, [r12, #12]
    ldr r1, [r12, #16]
    ldr r2, [r12, #20]
    ldr r3, [r12, #24]
    ldr r4, [r12, #28]
    ldr r5, [r12, #32]
    ldr r6, [r12, #36]
    ldr r7, [r12, #40]
    ldr r8, [r12, #44]
    ldr r9, [r12, #48]
    ldr r10, [r12, #52]
    ldr r11, [r12, #56]
    mov r12, #0
    movs pc, lr
.size arm32_enter_user, . - arm32_enter_user
.section .note.GNU-stack,"",%progbits
