# RISC-V 64-bit Boot Loader
# Entry point for RISC-V 64-bit architecture

.section .text.boot
.globl _start

_start:
    # OpenSBI enters the kernel in supervisor mode.
    csrci sstatus, 0x2
    mv tp, a0
    
    # Set trap vector (stvec for supervisor mode)
    la t0, trap_vector
    csrw stvec, t0
    
    # Initialize stack pointer
    la sp, _stack_top
    csrw sscratch, sp
    
    # Jump to kernel main
    li a0, 0
    li a1, 0
    call kernel_main
    
    # Halt if kernel returns
    wfi
    j .

.section .bss
.align 12
_stack_bottom:
    .space 0x8000
_stack_top:
