# RISC-V 64-bit Trap Handlers

.section .text
.globl trap_vector

trap_vector:
    # Save context
    addi sp, sp, -256
    sd ra, 0(sp)
    sd t0, 8(sp)
    sd t1, 16(sp)
    sd t2, 24(sp)
    sd s0, 32(sp)
    sd s1, 40(sp)
    
    # Call C handler
    csrr a0, scause
    csrr a1, sepc
    call riscv_trap_handler
    
    # Restore context
    ld ra, 0(sp)
    ld t0, 8(sp)
    ld t1, 16(sp)
    ld t2, 24(sp)
    ld s0, 32(sp)
    ld s1, 40(sp)
    addi sp, sp, 256
    
    sret
