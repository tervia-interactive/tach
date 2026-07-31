/*
 * userland/runtime.h - Embedded userspace bootstrap
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _USERLAND_RUNTIME_H
#define _USERLAND_RUNTIME_H

#include <term/tty.h>

/*
 * Starts PID 1 and the interactive shell using the stable syscall/VFS
 * interfaces. The current runtime is embedded in the kernel image; the
 * process and ABI boundary is intentionally ready for a later ring-3 ELF
 * transition once per-process page tables are available.
 */
int userland_bootstrap(tty_t* tty);

#endif /* _USERLAND_RUNTIME_H */
