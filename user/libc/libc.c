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
int unlink(const char* path) {
    return (int)raw_syscall(SYS_UNLINK, (long)path, 0, 0, 0, 0, 0);
}
int mkdir(const char* path, int mode) {
    return (int)raw_syscall(SYS_MKDIR, (long)path, mode, 0, 0, 0, 0);
}
int rmdir(const char* path) {
    return (int)raw_syscall(SYS_RMDIR, (long)path, 0, 0, 0, 0, 0);
}
int rename(const char* old_path, const char* new_path) {
    return (int)raw_syscall(SYS_RENAME, (long)old_path, (long)new_path, 0, 0, 0, 0);
}
int sync(void) { return (int)raw_syscall(SYS_SYNC, 0, 0, 0, 0, 0, 0); }
int mount(const char* source, const char* target, const char* fstype) {
    return (int)raw_syscall(SYS_MOUNT, (long)source, (long)target, (long)fstype, 0, 0, 0);
}
int umount(const char* target) {
    return (int)raw_syscall(SYS_UMOUNT, (long)target, 0, 0, 0, 0, 0);
}
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
int stat(const char* path, struct stat* info) {
    return (int)raw_syscall(SYS_STAT, (long)path, (long)info, 0, 0, 0, 0);
}
int fstat(int fd, struct stat* info) {
    return (int)raw_syscall(SYS_FSTAT, fd, (long)info, 0, 0, 0, 0);
}
ssize_t getdents(int fd, struct dirent* entries, size_t count) {
    return (ssize_t)raw_syscall(SYS_GETDENTS, fd, (long)entries, (long)count,
                                0, 0, 0);
}
int chdir(const char* path) {
    return (int)raw_syscall(SYS_CHDIR, (long)path, 0, 0, 0, 0, 0);
}
char* getcwd(char* buffer, size_t size) {
    long result = raw_syscall(SYS_GETCWD, (long)buffer, (long)size, 0, 0, 0, 0);
    return result < 0 ? (char*)0 : buffer;
}
int kill(pid_t pid, int sig) {
    return (int)raw_syscall(SYS_KILL, pid, sig, 0, 0, 0, 0);
}
int sigaction(int sig, const struct sigaction* action,
              struct sigaction* old_action) {
    return (int)raw_syscall(SYS_SIGACTION, sig, (long)action,
                            (long)old_action, 0, 0, 0);
}
void* mmap(void* address, size_t length, int protection, int flags,
           int fd, off_t offset) {
    long result = raw_syscall(SYS_MMAP, (long)address, (long)length,
                              protection, flags, fd, (long)offset);
    /* O kernel devolve -errno em caso de erro. */
    if (result < 0 && result > -4096) return MAP_FAILED;
    return (void*)result;
}
int munmap(void* address, size_t length) {
    return (int)raw_syscall(SYS_MUNMAP, (long)address, (long)length,
                            0, 0, 0, 0);
}
void* brk(void* address) {
    return (void*)raw_syscall(SYS_BRK, (long)address, 0, 0, 0, 0, 0);
}
void _exit(int status) {
    (void)raw_syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
    for (;;) __asm__ volatile("pause");
}

/* Alocador de first-fit sobre brk(). Os blocos formam uma lista ordenada por
 * endereco e contigua, entao free() consegue fundir vizinhos livres. */
struct block {
    size_t size;        /* bytes uteis (multiplo de 16) */
    struct block* next;
    int free;
} __attribute__((aligned(16)));

#define BLOCK_HEADER sizeof(struct block)
#define BLOCK_MIN_SPLIT 16u

static struct block* heap_head;
static struct block* heap_tail;
static unsigned char* heap_end;
static int heap_ready;

static int heap_init(void) {
    unsigned char* base = (unsigned char*)brk((void*)0);
    if (!base || (size_t)base > (size_t)-4096) return -1;
    unsigned char* aligned =
        (unsigned char*)(((size_t)base + 15u) & ~(size_t)15u);
    if (aligned != base && (unsigned char*)brk(aligned) != aligned) return -1;
    heap_end = aligned;
    heap_ready = 1;
    return 0;
}

static void block_split(struct block* block, size_t size) {
    if (block->size < size + BLOCK_HEADER + BLOCK_MIN_SPLIT) return;
    struct block* rest =
        (struct block*)((unsigned char*)block + BLOCK_HEADER + size);
    rest->size = block->size - size - BLOCK_HEADER;
    rest->free = 1;
    rest->next = block->next;
    block->next = rest;
    block->size = size;
    if (heap_tail == block) heap_tail = rest;
}

void* malloc(size_t size) {
    if (!size || size > (size_t)-1 / 2) return (void*)0;
    size = (size + 15u) & ~(size_t)15u;
    if (!heap_ready && heap_init() < 0) return (void*)0;

    for (struct block* block = heap_head; block; block = block->next) {
        if (block->free && block->size >= size) {
            block_split(block, size);
            block->free = 0;
            return (unsigned char*)block + BLOCK_HEADER;
        }
    }

    size_t total = BLOCK_HEADER + size;
    if ((size_t)heap_end + total < (size_t)heap_end) return (void*)0;
    unsigned char* requested = heap_end + total;
    if ((unsigned char*)brk(requested) != requested) return (void*)0;

    struct block* block = (struct block*)heap_end;
    block->size = size;
    block->next = (struct block*)0;
    block->free = 0;
    if (heap_tail) heap_tail->next = block; else heap_head = block;
    heap_tail = block;
    heap_end = requested;
    return (unsigned char*)block + BLOCK_HEADER;
}

void free(void* pointer) {
    if (!pointer) return;
    struct block* block = (struct block*)((unsigned char*)pointer - BLOCK_HEADER);
    /* Ignora ponteiros fora do heap e double free. */
    if ((unsigned char*)block < (unsigned char*)heap_head ||
        (unsigned char*)pointer >= heap_end || block->free) return;
    block->free = 1;
    for (struct block* cursor = heap_head; cursor && cursor->next;) {
        if (cursor->free && cursor->next->free) {
            struct block* absorbed = cursor->next;
            cursor->size += BLOCK_HEADER + absorbed->size;
            cursor->next = absorbed->next;
            if (heap_tail == absorbed) heap_tail = cursor;
        } else {
            cursor = cursor->next;
        }
    }
}

void* realloc(void* pointer, size_t size) {
    if (!pointer) return malloc(size);
    if (!size) { free(pointer); return (void*)0; }
    if (size > (size_t)-1 / 2) return (void*)0;
    struct block* block = (struct block*)((unsigned char*)pointer - BLOCK_HEADER);
    if (block->size >= ((size + 15u) & ~(size_t)15u)) return pointer;
    void* fresh = malloc(size);
    if (!fresh) return (void*)0;
    memcpy(fresh, pointer, block->size);
    free(pointer);
    return fresh;
}

void* calloc(size_t count, size_t size) {
    if (count && size > (size_t)-1 / count) return (void*)0;
    size_t total = count * size;
    void* pointer = malloc(total);
    if (pointer) memset(pointer, 0, total);
    return pointer;
}

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

NO_LOOP_PATTERNS void* memchr(const void* source, int value, size_t count) {
    const unsigned char* s = (const unsigned char*)source;
    while (count--) {
        if (*s == (unsigned char)value) return (void*)s;
        s++;
    }
    return (void*)0;
}
NO_LOOP_PATTERNS size_t strnlen(const char* text, size_t limit) {
    size_t length = 0;
    while (text && length < limit && text[length]) length++;
    return length;
}
NO_LOOP_PATTERNS int strncmp(const char* left, const char* right,
                             size_t count) {
    while (count && *left && *left == *right) { left++; right++; count--; }
    if (!count) return 0;
    return (unsigned char)*left - (unsigned char)*right;
}
NO_LOOP_PATTERNS char* strncpy(char* destination, const char* source,
                               size_t count) {
    size_t i = 0;
    for (; i < count && source[i]; i++) destination[i] = source[i];
    for (; i < count; i++) destination[i] = '\0';
    return destination;
}
NO_LOOP_PATTERNS char* strcat(char* destination, const char* source) {
    char* end = destination;
    while (*end) end++;
    while ((*end++ = *source++)) {}
    return destination;
}
NO_LOOP_PATTERNS char* strchr(const char* text, int ch) {
    char target = (char)ch;
    for (;; text++) {
        if (*text == target) return (char*)text;
        if (!*text) return (char*)0;
    }
}
NO_LOOP_PATTERNS char* strrchr(const char* text, int ch) {
    char target = (char)ch;
    const char* last = (const char*)0;
    for (;; text++) {
        if (*text == target) last = text;
        if (!*text) break;
    }
    return (char*)last;
}
NO_LOOP_PATTERNS char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return (char*)0;
}
char* strdup(const char* text) {
    size_t length = strlen(text) + 1;
    char* copy = (char*)malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}
int atoi(const char* text) {
    while (*text == ' ' || *text == '\t' || *text == '\n') text++;
    int negative = 0;
    if (*text == '-') { negative = 1; text++; }
    else if (*text == '+') text++;
    unsigned int value = 0;
    while (*text >= '0' && *text <= '9')
        value = value * 10u + (unsigned int)(*text++ - '0');
    return negative ? (int)(0u - value) : (int)value;
}
