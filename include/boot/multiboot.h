/*
 * boot/multiboot.h - Normalized Multiboot memory-map handoff
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _BOOT_MULTIBOOT_H
#define _BOOT_MULTIBOOT_H

#include <kernel/types.h>

int multiboot_parse(uint32_t magic, uintptr_t info);
size_t multiboot_module_count(void);
int multiboot_get_module(size_t index, const void** address, size_t* size,
                         const char** name);

#endif /* _BOOT_MULTIBOOT_H */
