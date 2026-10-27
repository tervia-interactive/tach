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

static struct process g_mock_process;
static struct vmm_context g_mock_vmm;

/* Host mocks for process and hardware functions */
struct process* process_get_current(void) {
    return &g_mock_process;
}

void kernel_panic_with_code(const char* reason, int code) {
    (void)reason;
    (void)code;
    __builtin_trap();
}

int main(void) {
    /* Initialize mock process */
    memset(&g_mock_process, 0, sizeof(g_mock_process));
    memset(&g_mock_vmm, 0, sizeof(g_mock_vmm));

    g_mock_process.pid = 1;
    g_mock_process.user_mode = true;
    g_mock_process.mm = &g_mock_vmm;
    g_mock_process.user_stack_top = PROCESS_USER_STACK_TOP;
    g_mock_process.user_stack_bottom = PROCESS_USER_STACK_TOP - PAGE_SIZE;
    g_mock_process.brk_start = 0x00410000;
    g_mock_process.brk_end = 0x00420000;

    syscall_init();

    /* Test pointer definitions */
    uint64_t null_ptr = 0;
    uint64_t kernel_ptr = 0x00100000ULL;        /* Kernel base (1MB) */
    uint64_t kernel_ptr_high = 0xC0000000ULL;   /* Higher-half kernel */
    uint64_t invalid_user_ptr = 0x00700000ULL;  /* Unmapped user pointer */
    uint64_t wrap_user_ptr = 0x7FFFFFFFULL;     /* Boundary/overflow pointer */

    /* =========================================================================
     * 1. Test SYS_READ with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_READ, 0, null_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, kernel_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, kernel_ptr_high, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, invalid_user_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_READ, 0, wrap_user_ptr, 0x1000, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 2. Test SYS_WRITE with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_WRITE, 1, null_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, kernel_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, kernel_ptr_high, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, invalid_user_ptr, 10, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WRITE, 1, wrap_user_ptr, 0x1000, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 3. Test SYS_OPEN with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_OPEN, null_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_OPEN, kernel_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_OPEN, kernel_ptr_high, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_OPEN, invalid_user_ptr, 0, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 4. Test SYS_EXECVE with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_EXECVE, null_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, kernel_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, kernel_ptr_high, 0, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, invalid_user_ptr, 0, 0, 0, 0, 0) == -EFAULT);
    /* Execve with valid path pointer representation but invalid argv pointer */
    CHECK(syscall_dispatch(SYS_EXECVE, 0x00401000ULL, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_EXECVE, 0x00401000ULL, invalid_user_ptr, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 5. Test SYS_WAITPID with NULL, kernel, and invalid user pointers for status
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_WAITPID, 100, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WAITPID, 100, kernel_ptr_high, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_WAITPID, 100, invalid_user_ptr, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 6. Test SYS_SIGACTION with NULL, kernel, and invalid user pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, kernel_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, kernel_ptr_high, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, invalid_user_ptr, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, 0, kernel_ptr, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_SIGACTION, 15, 0, invalid_user_ptr, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 7. Test SYS_MMAP and SYS_MUNMAP with invalid user/kernel pointers
     * ========================================================================= */
    CHECK(syscall_dispatch(SYS_MMAP, kernel_ptr, 4096, 1, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_MUNMAP, null_ptr, 4096, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_MUNMAP, kernel_ptr, 4096, 0, 0, 0, 0) == -EFAULT);
    CHECK(syscall_dispatch(SYS_MUNMAP, invalid_user_ptr, 4096, 0, 0, 0, 0) == -EFAULT);

    /* =========================================================================
     * 8. Verify helper functions directly
     * ========================================================================= */
    CHECK(!is_valid_user_range(NULL, 10));
    CHECK(!is_valid_user_range((const void*)kernel_ptr, 10));
    CHECK(!is_valid_user_range((const void*)kernel_ptr_high, 10));
    CHECK(!is_valid_user_range((const void*)wrap_user_ptr, 0x1000));
    CHECK(is_valid_user_range((const void*)0x00400000ULL, 1024));

    return 0;
}
