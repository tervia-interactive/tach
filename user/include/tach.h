#ifndef TACH_USER_H
#define TACH_USER_H

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef int pid_t;

#define SYS_EXIT 1
#define SYS_FORK 2
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_WAITPID 7
#define SYS_EXECVE 11
#define SYS_BRK 17
#define SYS_GETPID 20

ssize_t read(int fd, void* buffer, size_t count);
ssize_t write(int fd, const void* buffer, size_t count);
int open(const char* path, int flags);
int close(int fd);
pid_t fork(void);
int execve(const char* path, char* const argv[], char* const envp[]);
pid_t waitpid(pid_t pid, int* status, int options);
pid_t getpid(void);
void* brk(void* address);
void* malloc(size_t size);
void free(void* pointer);
void _exit(int status) __attribute__((noreturn));
size_t strlen(const char* text);
int strcmp(const char* left, const char* right);
char* strcpy(char* destination, const char* source);
void puts_fd(int fd, const char* text);

#endif
