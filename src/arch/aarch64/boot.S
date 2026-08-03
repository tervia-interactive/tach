.section .text.boot,"ax"
.global _start
.type _start, %function
_start:
    /* QEMU and firmware may hand a bare-metal payload to EL2.  Establish a
     * 64-bit EL1 host with direct access to the physical timer before any C
     * code touches EL1 system registers. */
    mrs x0, CurrentEL
    cmp x0, #0x8
    b.ne 1f
    mov x0, #1
    lsl x0, x0, #31
    msr hcr_el2, x0
    mrs x0, cnthctl_el2
    orr x0, x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    adr x0, 1f
    msr elr_el2, x0
    mov x0, #0x3c5
    msr spsr_el2, x0
    eret
1:
    mrs x0, CurrentEL
    cmp x0, #0x4
    b.ne loop
    mrs x0, mpidr_el1
    and x0, x0, #0xFFFFFF
    cbnz x0, secondary_cpu
    ldr x1, =stack_top
    mov sp, x1
    ldr x1, =exception_vector_el1
    msr vbar_el1, x1
    isb
    mov x0, xzr
    mov x1, xzr
    bl kernel_main
loop: wfi; b loop
secondary_cpu:
    b loop
.size _start, . - _start

.section .bss
.align 4
stack_bottom: .skip 32768
stack_top:
