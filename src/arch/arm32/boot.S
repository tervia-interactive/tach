.section .text
.global _start
.type _start, %function
_start:
    mrs r0, cpsr
    bic r0, r0, #0x1F
    orr r0, r0, #0xD3
    msr cpsr_c, r0
    ldr sp, =stack_top
    bl kernel_main
loop: wfe; b loop
.size _start, . - _start

.section .bss
.align 4
stack_bottom: .skip 16384
stack_top:
