/*
 * boot/sbi.h - Supervisor Binary Interface (RISC-V)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _BOOT_SBI_H
#define _BOOT_SBI_H

#include <kernel/types.h>

#define SBI_EXT_ID_BASE 0x10
#define SBI_FID_BASE_GET_SPEC_VERSION 0
#define SBI_FID_BASE_GET_IMPL_ID 1
#define SBI_FID_BASE_GET_IMPL_VERSION 2

#define SBI_EXT_ID_TIMER 0x54494D45
#define SBI_FID_TIMER_SET_TIMER 0

#define SBI_EXT_ID_IPI 0x735049
#define SBI_FID_IPI_SEND_IPI 0

#define SBI_EXT_ID_RFENCE 0x52464E43
#define SBI_FID_RFENCE_REMOTE_FENCE_I 0
#define SBI_FID_RFENCE_SFENCE_VMA 1
#define SBI_EXT_ID_HSM 0x48534D
#define SBI_FID_HSM_HART_START 0
#define SBI_FID_HSM_HART_STOP 1
#define SBI_FID_HSM_HART_GET_STATUS 2

struct sbi_return {
    long error;
    long value;
};

struct sbi_call {
    long arg0;
    long arg1;
    long arg2;
    long arg3;
    long arg4;
    long arg5;
    long extension_id;
    long function_id;
};

struct sbi_return sbi_ecall(long eid, long fid, long a0, long a1, long a2,
                            long a3, long a4, long a5);

#endif /* _BOOT_SBI_H */
