/*
 * src/arch/riscv64/aclint.c - ACLINT (MTIME/MSI for timer/IPI)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <hal/time.h>
#include <hal/irq.h>

void aclint_init(void) {
    /* Initialize ACLINT MTIME and MSI */
}

uint64_t aclint_get_mtime(void) {
    uint64_t mtime;
    __asm__ volatile("csrr %0, time" : "=r"(mtime));
    return mtime;
}

void aclint_send_ipi(uint32_t hart_id) {
    /* Send IPI to specified hart */
    (void)hart_id;
}
