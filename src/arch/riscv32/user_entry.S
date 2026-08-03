.section .text
.global riscv32_enter_user
.type riscv32_enter_user, @function
riscv32_enter_user:
    mv t6, a0
    csrw sscratch, sp
    lw t0, 0(t6)
    csrw sepc, t0
    csrr t0, sstatus
    li t1, -257
    and t0, t0, t1
    ori t0, t0, 32
    csrw sstatus, t0
    lw ra, 16(t6)
    lw gp, 24(t6)
    lw tp, 28(t6)
    lw s0, 44(t6)
    lw s1, 48(t6)
    lw a0, 52(t6)
    lw a1, 56(t6)
    lw a2, 60(t6)
    lw a3, 64(t6)
    lw a4, 68(t6)
    lw a5, 72(t6)
    lw a6, 76(t6)
    lw a7, 80(t6)
    lw s2, 84(t6)
    lw s3, 88(t6)
    lw s4, 92(t6)
    lw s5, 96(t6)
    lw s6, 100(t6)
    lw s7, 104(t6)
    lw s8, 108(t6)
    lw s9, 112(t6)
    lw s10, 116(t6)
    lw s11, 120(t6)
    lw sp, 4(t6)
    li t0, 0
    li t1, 0
    li t2, 0
    li t3, 0
    li t4, 0
    li t5, 0
    li t6, 0
    sret
.size riscv32_enter_user, . - riscv32_enter_user
.section .note.GNU-stack,"",@progbits
