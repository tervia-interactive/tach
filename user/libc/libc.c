#include <tach.h>

static long raw_syscall(long number, long a1, long a2, long a3,
                        long a4, long a5, long a6) {
#if defined(__x86_64__)
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    register long rax __asm__("rax") = number;
    register long rdi __asm__("rdi") = a1;
    register long rsi __asm__("rsi") = a2;
    register long rdx __asm__("rdx") = a3;
    __asm__ volatile("int $0x80" : "+a"(rax)
                     : "D"(rdi), "S"(rsi), "d"(rdx),
                       "r"(r10), "r"(r8), "r"(r9) : "memory", "cc");
    return rax;
#else
    register long eax __asm__("eax") = number;
    register long ebx __asm__("ebx") = a1;
    register long ecx __asm__("ecx") = a2;
    register long edx __asm__("edx") = a3;
    register long esi __asm__("esi") = a4;
    register long edi __asm__("edi") = a5;
    register long ebp __asm__("ebp") = a6;
    __asm__ volatile("int $0x80" : "+a"(eax)
                     : "b"(ebx), "c"(ecx), "d"(edx),
                       "S"(esi), "D"(edi), "r"(ebp) : "memory", "cc");
    return eax;
#endif
}

ssize_t read(int fd, void* buffer, size_t count) {
    return (ssize_t)raw_syscall(SYS_READ, fd, (long)buffer, count, 0, 0, 0);
}
ssize_t write(int fd, const void* buffer, size_t count) {
    return (ssize_t)raw_syscall(SYS_WRITE, fd, (long)buffer, count, 0, 0, 0);
}
int open(const char* path, int flags) {
    return (int)raw_syscall(SYS_OPEN, (long)path, flags, 0, 0, 0, 0);
}
int close(int fd) { return (int)raw_syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0); }
pid_t fork(void) { return (pid_t)raw_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0); }
int execve(const char* path, char* const argv[], char* const envp[]) {
    return (int)raw_syscall(SYS_EXECVE, (long)path, (long)argv,
                            (long)envp, 0, 0, 0);
}
pid_t waitpid(pid_t pid, int* status, int options) {
    return (pid_t)raw_syscall(SYS_WAITPID, pid, (long)status, options, 0, 0, 0);
}
pid_t getpid(void) { return (pid_t)raw_syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0); }
void* brk(void* address) {
    return (void*)raw_syscall(SYS_BRK, (long)address, 0, 0, 0, 0, 0);
}
void _exit(int status) {
    (void)raw_syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
    for (;;) __asm__ volatile("pause");
}

static unsigned char* heap_cursor;
static unsigned char* heap_end;
void* malloc(size_t size) {
    if (!size) return (void*)0;
    size = (size + 15u) & ~(size_t)15u;
    if (!heap_cursor) heap_cursor = heap_end = (unsigned char*)brk((void*)0);
    if (!heap_cursor || heap_cursor + size < heap_cursor) return (void*)0;
    if (heap_cursor + size > heap_end) {
        unsigned char* requested = heap_cursor + size;
        void* result = brk(requested);
        if (result != requested) return (void*)0;
        heap_end = requested;
    }
    void* result = heap_cursor;
    heap_cursor += size;
    return result;
}
void free(void* pointer) { (void)pointer; }

size_t strlen(const char* text) {
    size_t length = 0; while (text && text[length]) length++; return length;
}
int strcmp(const char* left, const char* right) {
    while (*left && *left == *right) { left++; right++; }
    return (unsigned char)*left - (unsigned char)*right;
}
char* strcpy(char* destination, const char* source) {
    char* result = destination; while ((*destination++ = *source++)) {} return result;
}
void puts_fd(int fd, const char* text) { (void)write(fd, text, strlen(text)); }
