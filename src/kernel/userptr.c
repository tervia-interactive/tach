/*
 * kernel/userptr.c - User pointer validation implementation
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#include <kernel/types.h>
#include <kernel/errno.h>
#include <kernel/userptr.h>

int is_valid_user_range(const void* ptr, size_t size) {
    if (!ptr) return 0;
    uintptr_t addr = (uintptr_t)ptr;

    /* Check for integer overflow */
    if (size > 0 && (addr + size < addr)) {
        return 0;
    }

    /* Address must be strictly within user space bounds */
    if (addr < USER_SPACE_START || addr >= USER_SPACE_END) {
        return 0;
    }

    if (size > 0) {
        uintptr_t end_addr = addr + size - 1;
        if (end_addr < USER_SPACE_START || end_addr >= USER_SPACE_END) {
            return 0;
        }
    }

    return 1;
}

int is_valid_kernel_range(const void* ptr, size_t size) {
    if (!ptr) return 0;
    uintptr_t addr = (uintptr_t)ptr;

    if (size > 0 && (addr + size < addr)) {
        return 0;
    }

    /* Kernel range is below USER_SPACE_START or >= USER_SPACE_END */
    bool start_in_kernel = (addr >= KERNEL_BASE && addr < USER_SPACE_START) ||
                           (addr >= USER_SPACE_END);
    if (!start_in_kernel) {
        return 0;
    }

    if (size > 0) {
        uintptr_t end_addr = addr + size - 1;
        bool end_in_kernel = (end_addr >= KERNEL_BASE && end_addr < USER_SPACE_START) ||
                             (end_addr >= USER_SPACE_END);
        if (!end_in_kernel) {
            return 0;
        }
    }

    return 1;
}

#ifdef TACH_HOST_TEST
#include <kernel/string.h>
#include <mm/vmm.h>

/*
 * Host tests pass fake user addresses (e.g. 0x401000) that are not mapped in
 * the host process. The real kernel gets -EFAULT from the page-table walk, so
 * emulate that here: let the host kernel fault-check the range via a pipe
 * (write() returns -1/EFAULT instead of crashing on an unreadable address).
 * Declared by hand because host libc headers conflict with kernel/types.h.
 */
extern int pipe(int fds[2]);
extern long write(int fd, const void* buf, unsigned long n);
extern long read(int fd, void* buf, unsigned long n);
extern int close(int fd);

static int host_range_readable(const void* ptr, size_t size) {
    int fds[2];
    const volatile char* p = (const volatile char*)ptr;
    char sink;
    int ok = 1;

    if (pipe(fds) != 0) return 0;
    for (size_t i = 0; i < size && ok; i++) {
        if (write(fds[1], (const void*)(p + i), 1) != 1 ||
            read(fds[0], &sink, 1) != 1) {
            ok = 0;
        }
    }
    close(fds[0]);
    close(fds[1]);
    return ok;
}

__attribute__((weak)) int vmm_copy_from_user(void* destination, struct vmm_context* ctx,
                                             const void* source, size_t size) {
    (void)ctx;
    if (!destination || (!source && size)) return -EFAULT;
    if (!host_range_readable(source, size)) return -EFAULT;
    memcpy(destination, source, size);
    return 0;
}

__attribute__((weak)) int vmm_copy_to_user(struct vmm_context* ctx, void* destination,
                                           const void* source, size_t size) {
    (void)ctx;
    if ((!destination && size) || !source) return -EFAULT;
    /* A pipe cannot probe writability; fake user pointers are never mapped. */
    if (!host_range_readable(destination, size)) return -EFAULT;
    memcpy(destination, source, size);
    return 0;
}

__attribute__((weak)) int vmm_copy_string_from_user(char* destination, size_t capacity,
                                                    struct vmm_context* ctx,
                                                    const char* source) {
    (void)ctx;
    if (!destination || !capacity || !source) return -EFAULT;
    for (size_t i = 0; i < capacity; i++) {
        if (!host_range_readable(source + i, 1)) return -EFAULT;
        destination[i] = source[i];
        if (!source[i]) return 0;
    }
    return -ENAMETOOLONG;
}
#endif
