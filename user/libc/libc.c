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
pid_t getppid(void) { return (pid_t)raw_syscall(SYS_GETPPID, 0, 0, 0, 0, 0, 0); }
uid_t getuid(void) { return (uid_t)raw_syscall(SYS_GETUID, 0, 0, 0, 0, 0, 0); }
int pipe(int fds[2]) { return (int)raw_syscall(SYS_PIPE, (long)fds, 0, 0, 0, 0, 0); }
int dup(int fd) { return (int)raw_syscall(SYS_DUP, fd, 0, 0, 0, 0, 0); }
int dup2(int old_fd, int new_fd) {
    return (int)raw_syscall(SYS_DUP2, old_fd, new_fd, 0, 0, 0, 0);
}
off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)raw_syscall(SYS_LSEEK, fd, (long)offset, whence, 0, 0, 0);
}
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

/* Com -O2 o GCC pode gerar chamadas a memcpy/memset/memmove/memcmp mesmo em
 * codigo freestanding; o atributo impede que estes loops virem chamadas a si
 * mesmos. */
#define NO_LOOP_PATTERNS \
    __attribute__((optimize("no-tree-loop-distribute-patterns")))

NO_LOOP_PATTERNS void* memcpy(void* destination, const void* source,
                              size_t count) {
    unsigned char* d = (unsigned char*)destination;
    const unsigned char* s = (const unsigned char*)source;
    while (count--) *d++ = *s++;
    return destination;
}
NO_LOOP_PATTERNS void* memmove(void* destination, const void* source,
                               size_t count) {
    unsigned char* d = (unsigned char*)destination;
    const unsigned char* s = (const unsigned char*)source;
    if (d < s) {
        while (count--) *d++ = *s++;
    } else {
        d += count; s += count;
        while (count--) *--d = *--s;
    }
    return destination;
}
NO_LOOP_PATTERNS void* memset(void* destination, int value, size_t count) {
    unsigned char* d = (unsigned char*)destination;
    while (count--) *d++ = (unsigned char)value;
    return destination;
}
NO_LOOP_PATTERNS int memcmp(const void* left, const void* right,
                            size_t count) {
    const unsigned char* a = (const unsigned char*)left;
    const unsigned char* b = (const unsigned char*)right;
    while (count--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}

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
