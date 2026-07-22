.section .text
.global _start
.type _start, %function
_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xFFFFFF
    cbnz x0, secondary_cpu
    ldr x1, =stack_top
    mov sp, x1
    bl kernel_main
loop: wfi; b loop
secondary_cpu:
    b loop
.size _start, . - _start

.section .bss
.align 4
stack_bottom: .skip 32768
stack_top:
