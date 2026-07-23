/**
 * @file unistd.h
 * @brief POSIX standard symbolic constants and types
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <stdint.h>
#include <stddef.h>

/* File access modes */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

/* Seek offsets */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Process types */
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
typedef int off_t;
typedef int ssize_t;

/* Function declarations */
_Noreturn void exit(int status);
pid_t getpid(void);
ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);
int open(const char *pathname, int flags, ...);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);

#endif /* _UNISTD_H_ */
