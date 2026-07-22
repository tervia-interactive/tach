# RISC-V 32-bit Boot Loader
# Entry point for RISC-V 32-bit architecture

.section .text.boot
.globl _start

_start:
    # Disable interrupts
    csrci mstatus, 0x8
    
    # Set trap vector
    la t0, trap_vector
    csrw mtvec, t0
    
    # Initialize stack pointer
    la sp, _stack_top
    
    # Jump to kernel main
    call kernel_main
    
    # Halt if kernel returns
    wfi
    j .

.section .bss
.align 12
_stack_bottom:
    .space 0x4000
_stack_top:
