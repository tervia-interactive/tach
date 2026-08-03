.section .text
.align 5
.global exception_vector
exception_vector:
    b arm_hang
    b arm_hang
    b arm_hang
    b arm_hang
    b arm_hang
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

arm_hang:
    cpsid if
1:  wfi
    b 1b
