/*
 * userland/runtime.h - Embedded userspace bootstrap
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _USERLAND_RUNTIME_H
#define _USERLAND_RUNTIME_H

#include <term/tty.h>

/*
 * Starts the embedded fallback PID 1 and shell through the syscall/VFS
 * interfaces. External ELF programs may replace this path when supplied
 * by an initrd.
 */
int userland_bootstrap(tty_t* tty);

#endif /* _USERLAND_RUNTIME_H */
