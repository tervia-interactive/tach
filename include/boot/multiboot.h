/*
 * boot/multiboot.h - Normalized Multiboot memory-map handoff
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _BOOT_MULTIBOOT_H
#define _BOOT_MULTIBOOT_H

#include <kernel/types.h>

int multiboot_parse(uint32_t magic, uintptr_t info);

#endif /* _BOOT_MULTIBOOT_H */
