# RISC-V 32-bit Boot Loader
# Entry point for RISC-V 32-bit architecture

.section .text.boot
.globl _start

_start:
    # OpenSBI enters the kernel in supervisor mode. Keep S-mode IRQs
    # masked until the HAL has installed the timer handler.
    csrci sstatus, 0x2
    
    # Set trap vector
    la t0, trap_vector
    csrw stvec, t0
    
    # Initialize stack pointer
    la sp, _stack_top
    
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
    .space 0x4000
_stack_top:
