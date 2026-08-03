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
#elif defined(__i386__)
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
#elif defined(__aarch64__)
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    register long x5 __asm__("x5") = a6;
    register long x8 __asm__("x8") = number;
    __asm__ volatile("svc #0" : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5),
                       "r"(x8) : "memory", "cc");
    return x0;
#elif defined(__arm__)
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    register long r4 __asm__("r4") = a5;
    register long r5 __asm__("r5") = a6;
    register long r7 __asm__("r7") = number;
    __asm__ volatile("svc #0" : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5),
                       "r"(r7) : "memory", "cc");
    return r0;
#elif defined(__riscv)
    register long a0 __asm__("a0") = a1;
    register long a1r __asm__("a1") = a2;
    register long a2r __asm__("a2") = a3;
    register long a3r __asm__("a3") = a4;
    register long a4r __asm__("a4") = a5;
    register long a5r __asm__("a5") = a6;
    register long a7 __asm__("a7") = number;
    __asm__ volatile("ecall" : "+r"(a0)
                     : "r"(a1r), "r"(a2r), "r"(a3r), "r"(a4r), "r"(a5r),
                       "r"(a7) : "memory");
    return a0;
#else
#error Unsupported userspace architecture
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
long lseek(int fd, long offset, int whence) {
    return raw_syscall(SYS_LSEEK, fd, offset, whence, 0, 0, 0);
}
int getdents(int fd, struct dirent* entries, size_t bytes) {
    return (int)raw_syscall(SYS_GETDENTS, fd, (long)entries, bytes, 0, 0, 0);
}
int mkdir(const char* path, int mode) {
    return (int)raw_syscall(SYS_MKDIR, (long)path, mode, 0, 0, 0, 0);
}
int unlink(const char* path) {
    return (int)raw_syscall(SYS_UNLINK, (long)path, 0, 0, 0, 0, 0);
}
int sync(void) { return (int)raw_syscall(SYS_SYNC, 0, 0, 0, 0, 0, 0); }
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
    for (;;) __asm__ volatile("" ::: "memory");
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
void* memcpy(void* destination, const void* source, size_t size) {
    unsigned char* out = destination; const unsigned char* in = source;
    while (size--) *out++ = *in++;
    return destination;
}
void* memset(void* destination, int value, size_t size) {
    unsigned char* out = destination;
    while (size--) *out++ = (unsigned char)value;
    return destination;
}
void puts_fd(int fd, const char* text) { (void)write(fd, text, strlen(text)); }
