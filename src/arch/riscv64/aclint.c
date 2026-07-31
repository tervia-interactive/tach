/*
 * src/arch/riscv64/aclint.c - ACLINT (MTIME/MSI for timer/IPI)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <hal/time.h>
#include <hal/irq.h>

void aclint_init(void) {
    /* OpenSBI owns the machine-mode ACLINT and exposes it through SBI. */
}

uint64_t aclint_get_mtime(void) {
    uint64_t mtime;
    __asm__ volatile("csrr %0, time" : "=r"(mtime));
    return mtime;
}

void aclint_set_timer(uint64_t deadline) {
    register uintptr_t a0 __asm__("a0") = (uintptr_t)deadline;
    register uintptr_t a6 __asm__("a6") = 0;
    register uintptr_t a7 __asm__("a7") = 0x54494d45u;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a6), "r"(a7) : "memory");
}

void aclint_send_ipi(uint32_t hart_id) {
    register uintptr_t a0 __asm__("a0") = (uintptr_t)1 << hart_id;
    register uintptr_t a1 __asm__("a1") = 0;
    register uintptr_t a6 __asm__("a6") = 0;
    register uintptr_t a7 __asm__("a7") = 0x735049u;
    __asm__ volatile("ecall" : "+r"(a0)
                     : "r"(a1), "r"(a6), "r"(a7) : "memory");
}
