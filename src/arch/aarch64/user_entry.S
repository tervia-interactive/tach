.section .text
.global aarch64_enter_user
.type aarch64_enter_user, %function
aarch64_enter_user:
    mov x18, x0
    ldr x1, [x18, #8]
    msr sp_el0, x1
    ldr x1, [x18, #0]
    msr elr_el1, x1
    mov x1, xzr
    msr spsr_el1, x1
    ldp x0, x1, [x18, #24]
    ldp x2, x3, [x18, #40]
    ldp x4, x5, [x18, #56]
    ldp x6, x7, [x18, #72]
    ldp x8, x9, [x18, #88]
    ldp x10, x11, [x18, #104]
    ldp x12, x13, [x18, #120]
    ldp x14, x15, [x18, #136]
    mov x16, xzr
    mov x17, xzr
    mov x18, xzr
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr
    mov x27, xzr
    mov x28, xzr
    mov x29, xzr
    mov x30, xzr
    eret
.size aarch64_enter_user, . - aarch64_enter_user
.section .note.GNU-stack,"",%progbits
