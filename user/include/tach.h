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
#define SYS_LSEEK 19
#define SYS_GETPID 20
#define SYS_UNLINK 10
#define SYS_MKDIR 39
#define SYS_GETDENTS 78
#define SYS_SYNC 87

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#define O_APPEND 0x0008
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define VNODE_FILE 1
#define VNODE_DIR 2

struct dirent {
    unsigned int d_ino, d_off; unsigned short d_reclen;
    unsigned char d_type; char d_name[256];
};

ssize_t read(int fd, void* buffer, size_t count);
ssize_t write(int fd, const void* buffer, size_t count);
int open(const char* path, int flags);
int close(int fd);
long lseek(int fd, long offset, int whence);
int getdents(int fd, struct dirent* entries, size_t bytes);
int mkdir(const char* path, int mode);
int unlink(const char* path);
int sync(void);
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
void* memcpy(void* destination, const void* source, size_t size);
void* memset(void* destination, int value, size_t size);
void puts_fd(int fd, const char* text);

#endif
