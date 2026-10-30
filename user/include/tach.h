#ifndef TACH_USER_H
#define TACH_USER_H

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef long off_t;

#define SYS_EXIT 1
#define SYS_FORK 2
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_WAITPID 7
#define SYS_UNLINK 10
#define SYS_EXECVE 11
#define SYS_BRK 17
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_GETUID 24
#define SYS_SIGACTION 27
#define SYS_KILL 37
#define SYS_DUP 41
#define SYS_PIPE 42
#define SYS_DUP2 63
#define SYS_GETPPID 64
#define SYS_MMAP 90
#define SYS_MUNMAP 91

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Flags de open() (mesmos valores de fs/vfs.h). */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_APPEND 0x0008
#define O_CREAT 0x0200
#define O_TRUNC 0x0400

#ifndef NULL
#define NULL ((void*)0)
#endif

/* Sinais (mesmos numeros de kernel/signal.h). */
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)

/* Layout identico a sigaction_t do kernel (sigset de 8 x 32 bits). */
typedef struct {
    unsigned int bits[8];
} sigset_t;

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

/* mmap: o kernel so atende mapeamentos anonimos (flags/fd/offset ignorados). */
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void*)-1)

ssize_t read(int fd, void* buffer, size_t count);
ssize_t write(int fd, const void* buffer, size_t count);
int open(const char* path, int flags);
int close(int fd);
int unlink(const char* path);
pid_t fork(void);
int execve(const char* path, char* const argv[], char* const envp[]);
pid_t waitpid(pid_t pid, int* status, int options);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
int pipe(int fds[2]);
int dup(int fd);
int dup2(int old_fd, int new_fd);
int kill(pid_t pid, int sig);
int sigaction(int sig, const struct sigaction* action,
              struct sigaction* old_action);
void* mmap(void* address, size_t length, int protection, int flags,
           int fd, off_t offset);
int munmap(void* address, size_t length);
off_t lseek(int fd, off_t offset, int whence);
void* brk(void* address);
void* malloc(size_t size);
void free(void* pointer);
void* calloc(size_t count, size_t size);
void* realloc(void* pointer, size_t size);
void _exit(int status) __attribute__((noreturn));
void* memcpy(void* destination, const void* source, size_t count);
void* memmove(void* destination, const void* source, size_t count);
void* memset(void* destination, int value, size_t count);
int memcmp(const void* left, const void* right, size_t count);
void* memchr(const void* source, int value, size_t count);
size_t strlen(const char* text);
size_t strnlen(const char* text, size_t limit);
int strcmp(const char* left, const char* right);
int strncmp(const char* left, const char* right, size_t count);
char* strcpy(char* destination, const char* source);
char* strncpy(char* destination, const char* source, size_t count);
char* strcat(char* destination, const char* source);
char* strchr(const char* text, int ch);
char* strrchr(const char* text, int ch);
char* strstr(const char* haystack, const char* needle);
char* strdup(const char* text);
int atoi(const char* text);
void puts_fd(int fd, const char* text);

#endif
