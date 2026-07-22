/*
 * proc/wait.h - waitpid()/zombie-reaping structures
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_WAIT_H
#define _PROC_WAIT_H

#include <kernel/types.h>

#define WNOHANG    1
#define WUNTRACED  2

pid_t wait(int* status);
pid_t waitpid(pid_t pid, int* status, int options);

#endif /* _PROC_WAIT_H */
