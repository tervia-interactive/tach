.section .text
.global riscv64_enter_user
.type riscv64_enter_user, @function
riscv64_enter_user:
    mv t6, a0
    csrw sscratch, sp
    ld t0, 0(t6)
    csrw sepc, t0
    csrr t0, sstatus
    li t1, -257
    and t0, t0, t1
    ori t0, t0, 32
    csrw sstatus, t0
    ld ra, 32(t6)
    ld gp, 48(t6)
    ld tp, 56(t6)
    ld s0, 88(t6)
    ld s1, 96(t6)
    ld a0, 104(t6)
    ld a1, 112(t6)
    ld a2, 120(t6)
    ld a3, 128(t6)
    ld a4, 136(t6)
    ld a5, 144(t6)
    ld a6, 152(t6)
    ld a7, 160(t6)
    ld s2, 168(t6)
    ld s3, 176(t6)
    ld s4, 184(t6)
    ld s5, 192(t6)
    ld s6, 200(t6)
    ld s7, 208(t6)
    ld s8, 216(t6)
    ld s9, 224(t6)
    ld s10, 232(t6)
    ld s11, 240(t6)
    ld sp, 8(t6)
    li t0, 0
    li t1, 0
    li t2, 0
    li t3, 0
    li t4, 0
    li t5, 0
    li t6, 0
    sret
.size riscv64_enter_user, . - riscv64_enter_user
.section .note.GNU-stack,"",@progbits
