.section .text
.align 5
.global exception_vector
exception_vector:
    b arm_hang
    b arm_hang
    b arm_svc_entry
    b arm_hang
    b arm_data_abort_entry
    b arm_hang
    b arm_irq_entry
    b arm_hang

arm_irq_entry:
    sub lr, lr, #4
    srsdb sp!, #0x13
    cps #0x13
    push {r0-r12, lr}
    bl arm_irq_handler
    pop {r0-r12, lr}
    rfeia sp!

arm_data_abort_entry:
    sub lr, lr, #8
    srsdb sp!, #0x13
    cps #0x13
    push {r0-r12, lr}
    bl arm_data_abort_handler
    pop {r0-r12, lr}
    rfeia sp!

arm_svc_entry:
    sub sp, sp, #68
    stmia sp, {r0-r12}
    add r0, sp, #52
    stmia r0, {r13-r14}^
    nop
    str lr, [sp, #60]
    mrs r0, spsr
    str r0, [sp, #64]
    mov r0, sp
    bl arm_syscall_handler
    ldr r0, [sp, #64]
    msr spsr_cxsf, r0
    add r0, sp, #52
    ldmia r0, {r13-r14}^
    nop
    ldr lr, [sp, #60]
    ldmia sp, {r0-r12}
    add sp, sp, #68
    movs pc, lr

arm_hang:
    cpsid if
1:  wfi
    b 1b
