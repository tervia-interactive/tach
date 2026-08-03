/* tach - ABI Header */
/* Kernel<->userland contract: struct/syscall version numbers */

#ifndef _KERNEL_ABI_H
#define _KERNEL_ABI_H

#include "kernel/types.h"

/* ABI version - increment on breaking changes */
#define TACH_ABI_VERSION_MAJOR 0
#define TACH_ABI_VERSION_MINOR 1

/* Syscall numbers */
#define SYS_EXIT        1
#define SYS_FORK        2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_WAITPID     7
#define SYS_EXECVE      11
#define SYS_BRK         17
#define SYS_GETPID      20
#define SYS_SIGACTION   27
#define SYS_KILL        37
#define SYS_MMAP        90
#define SYS_MUNMAP      91

/* Maximum syscall number */
#define SYS_MAX         100

/* Syscall convention markers */
#define SYSCALL_MARKER  0xDEADBEEF

#endif /* _KERNEL_ABI_H */
