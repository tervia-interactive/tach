# RISC-V 32-bit Trap Handlers

.section .text
.globl trap_vector

trap_vector:
    # Save context
    addi sp, sp, -128
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw s0, 16(sp)
    sw s1, 20(sp)
    
    # Call C handler
    csrr a0, mcause
    csrr a1, mepc
    call riscv_trap_handler
    
    # Restore context
    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)
    lw s0, 16(sp)
    lw s1, 20(sp)
    addi sp, sp, 128
    
    mret
