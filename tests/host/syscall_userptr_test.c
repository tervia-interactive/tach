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
#include <mm/pmm.h>
#include <fs/vfs.h>
#include <hal/paging.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        return __LINE__; \
    } \
} while (0)

#define MOCK_FRAMES 16

static uint8_t g_frames[MOCK_FRAMES][PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static bool g_frame_used[MOCK_FRAMES];
static struct process g_mock_process;
static struct vmm_context g_mock_vmm;

/* PMM Mocks */
phys_addr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < MOCK_FRAMES; i++) {
        if (!g_frame_used[i]) {
            g_frame_used[i] = true;
            return (phys_addr_t)(uintptr_t)g_frames[i];
        }
    }
    return 0;
}

void pmm_free_frame(phys_addr_t frame) {
    for (size_t i = 0; i < MOCK_FRAMES; i++) {
        if (frame == (phys_addr_t)(uintptr_t)g_frames[i]) {
            g_frame_used[i] = false;
        }
    }
}

bool pmm_retain_frame(phys_addr_t frame) {
    (void)frame;
    return true;
}

size_t pmm_frame_refcount(phys_addr_t frame) {
    (void)frame;
    return 1;
}

/* Arch Paging Mocks */
int arch_paging_init(void) { return 0; }
void* arch_paging_kernel_root(void) { return NULL; }
void* arch_paging_create_root(void) { return (void*)0x1000; }
void arch_paging_destroy_root(void* root) { (void)root; }
void arch_paging_switch(void* root) { (void)root; }
int arch_paging_map(void* root, uintptr_t virt, phys_addr_t phys, uint32_t flags) {
    (void)root; (void)virt; (void)phys; (void)flags;
    return 0;
}
int arch_paging_unmap(void* root, uintptr_t virt) {
    (void)root; (void)virt;
    return 0;
}

/* Console Mocks */
void hal_console_lock(void) {}
void hal_console_unlock(void) {}
void hal_console_putchar(char c) { (void)c; }
void hal_console_write(const char* s, size_t len) { (void)s; (void)len; }
void hal_console_clear(void) {}
char hal_console_getchar(void) { return '\n'; }

/* Process Mocks */
struct process* process_get_current(void) {
    return &g_mock_process;
}
void process_exit(int code) { (void)code; }
struct process* process_fork(struct process* parent) { (void)parent; return NULL; }
pid_t process_waitpid(pid_t pid, int* status, int options) {
    (void)pid; (void)status; (void)options;
    return -ECHILD;
}
int process_exec_image_args(struct process* proc, const void* image, size_t size,
                            const char* name, const char* const argv[], size_t argc) {
    (void)proc; (void)image; (void)size; (void)name; (void)argv; (void)argc;
    return -ENOENT;
}

/* VFS & FD Mocks */
struct fd_entry* fd_get(struct fd_table* table, int fd) {
    (void)table; (void)fd;
    return NULL;
}
int fd_alloc(struct fd_table* table, struct vnode* vnode, int flags) {
    (void)table; (void)vnode; (void)flags;
    return -1;
}
void fd_put(struct fd_table* table, int fd) { (void)table; (void)fd; }
struct vnode* vfs_open(const char* path, int flags) { (void)path; (void)flags; return NULL; }
int vfs_close(struct vnode* vnode) { (void)vnode; return 0; }
ssize_t vfs_read(struct vnode* vnode, void* buf, size_t count, off_t offset) {
    (void)vnode; (void)buf; (void)count; (void)offset;
    return -1;
}
ssize_t vfs_write(struct vnode* vnode, const void* buf, size_t count, off_t offset) {
    (void)vnode; (void)buf; (void)count; (void)offset;
    return -1;
}
int vfs_read_file(const char* path, const void** image, size_t* size) {
    (void)path; (void)image; (void)size;
    return -ENOENT;
}

/* Signal Mocks */
int signal_send(pid_t pid, int sig) { (void)pid; (void)sig; return 0; }
int signal_set_action(struct process* proc, int sig, const sigaction_t* act, sigaction_t* old) {
    (void)proc; (void)sig; (void)act; (void)old;
    return 0;
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
    uint64_t kernel_ptr_high = 0xFFFFFFFF80000000ULL; /* Higher-half kernel */
    uint64_t invalid_user_ptr = 0x00700000ULL;  /* Unmapped user pointer */
    uint64_t wrap_user_ptr = 0x00007FFFFFFFFFFFULL;   /* Boundary/overflow pointer */

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
