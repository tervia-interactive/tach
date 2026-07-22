/* tach Operating System - ABI Header */
/* Kernel<->userland contract: struct/syscall version numbers */

#ifndef _KERNEL_ABI_H
#define _KERNEL_ABI_H

#include "kernel/types.h"

/* ABI version - increment on breaking changes */
#define TACH_ABI_VERSION_MAJOR 0
#define TACH_ABI_VERSION_MINOR 1

/* Syscall numbers */
#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_STAT        4
#define SYS_FSTAT       5
#define SYS_LSEEK       6
#define SYS_MMAP        7
#define SYS_MUNMAP      8
#define SYS_BRK         9
#define SYS_EXIT        10
#define SYS_FORK        11
#define SYS_EXEC        12
#define SYS_WAITPID     13
#define SYS_GETPID      14
#define SYS_KILL        15
#define SYS_SIGACTION   16
#define SYS_THREAD_CREATE 17
#define SYS_THREAD_EXIT   18
#define SYS_PORT_CREATE   19
#define SYS_PORT_SEND     20
#define SYS_PORT_RECV     21
#define SYS_SEM_CREATE    22
#define SYS_SEM_ACQUIRE   23
#define SYS_SEM_RELEASE   24

/* Maximum syscall number */
#define SYS_MAX         100

/* Syscall convention markers */
#define SYSCALL_MARKER  0xDEADBEEF

#endif /* _KERNEL_ABI_H */
