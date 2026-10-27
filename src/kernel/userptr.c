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

__attribute__((weak)) int vmm_copy_from_user(void* destination, struct vmm_context* ctx,
                                             const void* source, size_t size) {
    (void)ctx;
    if (!destination || (!source && size)) return -EFAULT;
    memcpy(destination, source, size);
    return 0;
}

__attribute__((weak)) int vmm_copy_to_user(struct vmm_context* ctx, void* destination,
                                           const void* source, size_t size) {
    (void)ctx;
    if ((!destination && size) || !source) return -EFAULT;
    memcpy(destination, source, size);
    return 0;
}

__attribute__((weak)) int vmm_copy_string_from_user(char* destination, size_t capacity,
                                                    struct vmm_context* ctx,
                                                    const char* source) {
    (void)ctx;
    if (!destination || !capacity || !source) return -EFAULT;
    if (strlen(source) >= capacity) return -ENAMETOOLONG;
    strcpy(destination, source);
    return 0;
}
#endif
