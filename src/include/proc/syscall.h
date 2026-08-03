/*
 * proc/syscall.h - Syscall table and dispatcher
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_SYSCALL_H
#define _PROC_SYSCALL_H

#include <kernel/types.h>

#define SYS_EXIT        1
#define SYS_FORK        2
#define SYS_READ        3
#define SYS_WRITE       4
#define SYS_OPEN        5
#define SYS_CLOSE       6
#define SYS_WAITPID     7
#define SYS_CREAT       8
#define SYS_LINK        9
#define SYS_UNLINK      10
#define SYS_EXECVE      11
#define SYS_CHDIR       12
#define SYS_TIME        13
#define SYS_MKNOD       14
#define SYS_CHMOD       15
#define SYS_LCHOWN      16
#define SYS_BRK         17
#define SYS_GETPID      20
#define SYS_MOUNT       21
#define SYS_UMOUNT      22
#define SYS_SETUID      23
#define SYS_GETUID      24
#define SYS_PTRACE      26
#define SYS_SIGACTION   27
#define SYS_MMAP        90
#define SYS_MUNMAP      91
#define SYS_KILL        37
#define SYSCALL_TABLE_SIZE 128

typedef long (*syscall_handler_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void syscall_init(void);
long syscall_dispatch(int num, uint64_t arg1, uint64_t arg2, uint64_t arg3, 
                      uint64_t arg4, uint64_t arg5, uint64_t arg6);
void syscall_register(int num, syscall_handler_t handler);
const char* syscall_name(int num);

#endif /* _PROC_SYSCALL_H */
