/*
 * mm/mprotect.h - W^X enforcement helpers (no page both writable+executable)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _MM_MPROTECT_H
#define _MM_MPROTECT_H

#include <kernel/types.h>

struct vmm_context;

#define MPROT_NONE  0x0
#define MPROT_READ  0x1
#define MPROT_WRITE 0x2
#define MPROT_EXEC  0x4

int mprotect(void* addr, size_t len, int prot);
int enforce_wx_policy(struct vmm_context* ctx, void* addr, uint32_t flags);

#endif /* _MM_MPROTECT_H */
