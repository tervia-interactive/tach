# RISC-V 64-bit Boot Loader
# Entry point for RISC-V 64-bit architecture

.section .text.boot
.globl _start

_start:
    # Disable interrupts
    csrc mstatus, %t0
    
    # Set trap vector (stvec for supervisor mode)
    la t0, trap_vector
    csrw stvec, t0
    
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
    .space 0x8000
_stack_top:
