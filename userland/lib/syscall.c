/**
 * @file syscall.c
 * @brief System call stubs for tach userland
 * 
 * These functions will be connected to actual kernel syscalls.
 * For now, they are stubs that return error values.
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "tach.h"

/* Syscall numbers - must match kernel definitions */
#define SYS_EXIT      1
#define SYS_GETPID    2
#define SYS_WRITE     3
#define SYS_READ      4
#define SYS_OPEN      5
#define SYS_CLOSE     6

/* Architecture-specific syscall mechanism */
#if defined(__x86_64__)

static inline long syscall_noreturn(int num, unsigned long arg1) {
    __asm__ volatile (
        "syscall\n\t"
        :
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    __builtin_unreachable();
}

static inline long syscall1(int num, unsigned long arg1) {
    long ret;
    __asm__ volatile (
        "syscall\n\t"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#elif defined(__i386__)

static inline long syscall_noreturn(int num, unsigned long arg1) {
    __asm__ volatile (
        "int $0x80\n\t"
        :
        : "a"(num), "b"(arg1)
        : "memory"
    );
    __builtin_unreachable();
}

static inline long syscall1(int num, unsigned long arg1) {
    long ret;
    __asm__ volatile (
        "int $0x80\n\t"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
        : "memory"
    );
    return ret;
}

#elif defined(__aarch64__)

static inline long syscall1(int num, unsigned long arg1) {
    long ret;
    register long x0 asm("x0") = arg1;
    register long x8 asm("x8") = num;
    
    __asm__ volatile (
        "svc #0\n\t"
        : "=r"(x0)
        : "r"(x0), "r"(x8)
        : "memory"
    );
    
    ret = x0;
    return ret;
}

static inline void syscall_noreturn(int num, unsigned long arg1) {
    register long x0 asm("x0") = arg1;
    register long x8 asm("x8") = num;
    
    __asm__ volatile (
        "svc #0\n\t"
        :
        : "r"(x0), "r"(x8)
        : "memory"
    );
    
    __builtin_unreachable();
}

#elif defined(__riscv)

static inline long syscall1(int num, unsigned long arg1) {
    long ret;
    register long a0 asm("a0") = arg1;
    register long a7 asm("a7") = num;
    
    __asm__ volatile (
        "ecall\n\t"
        : "=r"(a0)
        : "r"(a0), "r"(a7)
        : "memory"
    );
    
    ret = a0;
    return ret;
}

static inline void syscall_noreturn(int num, unsigned long arg1) {
    register long a0 asm("a0") = arg1;
    register long a7 asm("a7") = num;
    
    __asm__ volatile (
        "ecall\n\t"
        :
        : "r"(a0), "r"(a7)
        : "memory"
    );
    
    __builtin_unreachable();
}

#else

/* Generic fallback - just return error */
static inline long syscall1(int num, unsigned long arg1) {
    (void)num;
    (void)arg1;
    return -1;
}

static inline void syscall_noreturn(int num, unsigned long arg1) {
    (void)num;
    (void)arg1;
    while(1);
}

#endif

/* ============================================
 * System Call Wrappers
 * ============================================ */

_Noreturn void exit(int status) {
    syscall_noreturn(SYS_EXIT, (unsigned long)status);
    __builtin_unreachable();
}

pid_t getpid(void) {
    return (pid_t)syscall1(SYS_GETPID, 0);
}

ssize_t write(fd_t fd, const void *buf, size_t count) {
    /* This is a simplified version - real impl needs more args */
    (void)fd;
    (void)buf;
    (void)count;
    return -1;  /* Stub */
}

ssize_t read(fd_t fd, void *buf, size_t count) {
    (void)fd;
    (void)buf;
    (void)count;
    return -1;  /* Stub */
}

fd_t open(const char *pathname, int flags) {
    (void)pathname;
    (void)flags;
    return -1;  /* Stub */
}

int close(fd_t fd) {
    (void)fd;
    return -1;  /* Stub */
}
