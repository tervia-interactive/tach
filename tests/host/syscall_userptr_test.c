/*
 * tests/host/syscall_userptr_test.c
 * Host test for user pointer validation in syscalls.
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <kernel/types.h>
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/userptr.h>
#include <proc/syscall.h>
#include <proc/process.h>
#include <mm/vmm.h>
#include <fs/vfs.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        return __LINE__; \
    } \
} while (0)

#ifndef TACH_HOST_TEST
#error This test must be compiled with -DTACH_HOST_TEST
#endif

/* These are provided by linking src/proc/process.c and others.
 * We need to ensure we don't redefine symbols. */

int main(void) {
    /* Initialize the process subsystem so process_get_current works */
    process_init();

    /* Create a process to act as current */
    struct process* proc = process_create("test-proc");
    CHECK(proc != NULL);

    proc->user_mode = true;
    process_set_current(proc);

    syscall_init();

    /* Test pointer definitions */
    uint64_t null_ptr = 0;
    uint64_t kernel_ptr = 0x00100000ULL;        /* Kernel base (1MB) */
    uint64_t kernel_ptr_high = 0xFFFFFFFF80000000ULL; /* Higher-half kernel */

    /* Use the actual USER_SPACE_END from userptr.h to test wrapping/out of bounds */
    uintptr_t wrap_user_ptr = USER_SPACE_END;

    /* =========================================================================
     * 1. Test SYS_READ with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_READ, 0, null_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, kernel_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, kernel_ptr_high, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, (uint64_t)wrap_user_ptr, 0x1000, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 2. Test SYS_WRITE with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_WRITE, 1, null_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, kernel_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, kernel_ptr_high, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, (uint64_t)wrap_user_ptr, 0x1000, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 3. Test SYS_OPEN with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_OPEN, null_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_OPEN, kernel_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_OPEN, kernel_ptr_high, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_OPEN, (uint64_t)wrap_user_ptr, 0, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 4. Test SYS_EXECVE with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_EXECVE, null_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, kernel_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, kernel_ptr_high, 0, 0, 0, 0, 0) == -EFAULT);

    /* Execve with valid path pointer representation but invalid argv pointer */
    /* Note: 0x00401000 is inside USER_SPACE_START and USER_SPACE_END */
    CHECK(syscall_dispatch(SYS_EXECVE, 0x00401000ULL, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, 0x00401000ULL, (uint64_t)wrap_user_ptr, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 5. Test SYS_WAITPID with NULL, kernel, and invalid user pointers for status
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_WAITPID, 100, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WAITPID, 100, kernel_ptr_high, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WAITPID, 100, (uint64_t)wrap_user_ptr, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 6. Test SYS_SIGACTION with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, kernel_ptr_high, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, (uint64_t)wrap_user_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, 0, kernel_ptr, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, 0, (uint64_t)wrap_user_ptr, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 7. Test SYS_MMAP and SYS_MUNMAP with invalid user/kernel pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_MMAP, kernel_ptr, 4096, 1, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_MUNMAP, null_ptr, 4096, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_MUNMAP, kernel_ptr, 4096, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_MUNMAP, (uint64_t)wrap_user_ptr, 4096, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 8. Verify helper functions directly
     * ========================================================================= */
    CHECK(!is_valid_user_range(NULL, 10));
    CHECK(!is_valid_user_range((const void*)kernel_ptr, 10));
    CHECK(!is_valid_user_range((const void*)kernel_ptr_high, 10));
    CHECK(!is_valid_user_range((const void*)wrap_user_ptr, 0x1000));
    /* Test valid user range - 0x00400000 is USER_SPACE_START */
    CHECK(is_valid_user_range((const void*)0x00400000ULL, 1024));

    return 0;
}
