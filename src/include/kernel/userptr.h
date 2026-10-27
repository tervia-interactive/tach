/*
 * kernel/userptr.h - User pointer validation helpers
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _KERNEL_USERPTR_H
#define _KERNEL_USERPTR_H

#include <kernel/types.h>

/* User space memory ranges */
#define KERNEL_BASE      0x00100000ULL
#define USER_SPACE_START 0x00400000ULL
#define USER_SPACE_END   0x00800000ULL
#define USER_STACK_TOP   0x007ff00000ULL
#define USER_STACK_MAX   (8u * 1024u * 1024u)

/* Helper macros to check if an address is in user space */
#define IS_USER_PTR(addr) ((uintptr_t)(addr) >= USER_SPACE_START && \
                           (uintptr_t)(addr) < USER_SPACE_END)
#define IS_KERNEL_PTR(addr) ((uintptr_t)(addr) >= KERNEL_BASE && \
                            (uintptr_t)(addr) < USER_SPACE_START)
#define IS_NULL_PTR(addr) ((uintptr_t)(addr) == 0)
#define IS_VALID_PTR(addr) (!IS_NULL_PTR(addr) && IS_USER_PTR(addr))

/* Helper macros for stack bounds */
#define IS_IN_USER_STACK(addr) ((uintptr_t)(addr) >= \
                                (USER_STACK_TOP - USER_STACK_MAX) && \
                                (uintptr_t)(addr) < USER_STACK_TOP)

/* Check if a range is valid for user space */
int is_valid_user_range(const void* ptr, size_t size);
int is_valid_kernel_range(const void* ptr, size_t size);

#endif /* _KERNEL_USERPTR_H */
