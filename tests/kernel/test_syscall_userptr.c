/*
 * tests/kernel/test_syscall_userptr.c
 * Kernel unit test for user pointer validation in syscalls.
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <kernel/types.h>
#include <kernel/errno.h>
#include <kernel/assert.h>
#include <kernel/userptr.h>
#include <proc/syscall.h>
#include <proc/process.h>
#include <mm/vmm.h>

#ifdef TACH_TEST

static void test_userptr_validation(void) {
    uint64_t null_ptr = 0;
    uint64_t kernel_ptr = (uint64_t)(uintptr_t)syscall_dispatch;
    uint64_t kernel_high = 0xC0000000ULL;
    uint64_t invalid_user = 0x00700000ULL;
    uint64_t overflow_ptr = 0x7FFFFFFFULL;

    /* Verify IS_USER_PTR, IS_KERNEL_PTR, and is_valid_user_range */
    KASSERT(!is_valid_user_range(NULL, 10));
    KASSERT(!is_valid_user_range((const void*)(uintptr_t)kernel_ptr, 10));
    KASSERT(!is_valid_user_range((const void*)(uintptr_t)kernel_high, 10));
    KASSERT(!is_valid_user_range((const void*)(uintptr_t)overflow_ptr, 0x1000));
    KASSERT(is_valid_user_range((const void*)0x00400000ULL, 100));

    /* SYS_READ invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_READ, 0, null_ptr, 10, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_READ, 0, kernel_ptr, 10, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_READ, 0, kernel_high, 10, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_READ, 0, invalid_user, 10, 0, 0, 0) == -EFAULT);

    /* SYS_WRITE invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_WRITE, 1, null_ptr, 10, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_WRITE, 1, kernel_ptr, 10, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_WRITE, 1, kernel_high, 10, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_WRITE, 1, invalid_user, 10, 0, 0, 0) == -EFAULT);

    /* SYS_OPEN invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_OPEN, null_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_OPEN, kernel_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_OPEN, kernel_high, 0, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_OPEN, invalid_user, 0, 0, 0, 0, 0) == -EFAULT);

    /* SYS_EXECVE invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_EXECVE, null_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_EXECVE, kernel_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_EXECVE, invalid_user, 0, 0, 0, 0, 0) == -EFAULT);

    /* SYS_WAITPID invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_WAITPID, 1, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_WAITPID, 1, invalid_user, 0, 0, 0, 0) == -EFAULT);

    /* SYS_SIGACTION invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_SIGACTION, 15, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_SIGACTION, 15, invalid_user, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_SIGACTION, 15, 0, kernel_ptr, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_SIGACTION, 15, 0, invalid_user, 0, 0, 0) == -EFAULT);

    /* SYS_MMAP & SYS_MUNMAP invalid pointer checks */
    KASSERT(syscall_dispatch(SYS_MMAP, kernel_ptr, 4096, 1, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_MUNMAP, null_ptr, 4096, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_MUNMAP, kernel_ptr, 4096, 0, 0, 0, 0) == -EFAULT);
    KASSERT(syscall_dispatch(SYS_MUNMAP, invalid_user, 4096, 0, 0, 0, 0) == -EFAULT);
}

void test_syscall_userptr_run_all(void) {
    kprintf("Running Syscall User Pointer Validation tests...\n");
    test_userptr_validation();
    kprintf("  [PASS] Syscall User Pointer Validation\n");
}

#endif /* TACH_TEST */
