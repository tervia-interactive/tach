/**
 * @file tach.h
 * @brief Main header for Tach userland C library
 * 
 * This is the primary include file for the tach standard C library.
 * It provides POSIX-like interfaces for user-space applications.
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#ifndef _TACH_H_
#define _TACH_H_

#include <stdint.h>
#include <stddef.h>

/* ============================================
 * Standard Types
 * ============================================ */

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef EOF
#define EOF (-1)
#endif

/* Basic types - defined locally to avoid system header dependencies */
typedef int fd_t;
typedef int pid_t;
typedef long ssize_t;

/* ============================================
 * Standard I/O Streams
 * ============================================ */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ============================================
 * System Calls Interface
 * ============================================ */

/**
 * @brief Exit the current process
 * @param status Exit status code
 */
_Noreturn void exit(int status);

/**
 * @brief Get the process ID of the calling process
 * @return Process ID
 */
pid_t getpid(void);

/**
 * @brief Write data to a file descriptor
 * @param fd File descriptor
 * @param buf Buffer containing data to write
 * @param count Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
ssize_t write(fd_t fd, const void *buf, size_t count);

/**
 * @brief Read data from a file descriptor
 * @param fd File descriptor
 * @param buf Buffer to store read data
 * @param count Maximum number of bytes to read
 * @return Number of bytes read, or -1 on error
 */
ssize_t read(fd_t fd, void *buf, size_t count);

/**
 * @brief Open a file
 * @param pathname Path to the file
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
 * @return File descriptor, or -1 on error
 */
fd_t open(const char *pathname, int flags);

/**
 * @brief Close a file descriptor
 * @param fd File descriptor to close
 * @return 0 on success, -1 on error
 */
int close(fd_t fd);

/* ============================================
 * Memory Management
 * ============================================ */

/**
 * @brief Allocate memory
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *malloc(size_t size);

/**
 * @brief Free allocated memory
 * @param ptr Pointer to memory to free
 */
void free(void *ptr);

/**
 * @brief Allocate and zero-initialize memory
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, or NULL on failure
 */
void *calloc(size_t nmemb, size_t size);

/**
 * @brief Reallocate memory
 * @param ptr Pointer to previously allocated memory
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *realloc(void *ptr, size_t size);

/* ============================================
 * String Functions
 * ============================================ */

/**
 * @brief Copy a string
 * @param dest Destination buffer
 * @param src Source string
 * @return Pointer to destination
 */
char *strcpy(char *dest, const char *src);

/**
 * @brief Concatenate strings
 * @param dest Destination buffer
 * @param src Source string to append
 * @return Pointer to destination
 */
char *strcat(char *dest, const char *src);

/**
 * @brief Get string length
 * @param s String to measure
 * @return Length of string (excluding null terminator)
 */
size_t strlen(const char *s);

/**
 * @brief Compare two strings
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strcmp(const char *s1, const char *s2);

/**
 * @brief Set memory to a value
 * @param s Destination buffer
 * @param c Value to set
 * @param n Number of bytes to set
 * @return Pointer to destination
 */
void *memset(void *s, int c, size_t n);

/**
 * @brief Copy memory
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to destination
 */
void *memcpy(void *dest, const void *src, size_t n);

/* ============================================
 * Format I/O
 * ============================================ */

/**
 * @brief Print formatted output to stdout
 * @param format Format string
 * @return Number of characters printed, or negative on error
 */
int printf(const char *format, ...);

/**
 * @brief Print formatted output to stderr
 * @param format Format string
 * @return Number of characters printed, or negative on error
 */
int fprintf(int fd, const char *format, ...);

/**
 * @brief Print a string followed by newline
 * @param s String to print
 * @return Non-negative on success, EOF on error
 */
int puts(const char *s);

/* ============================================
 * Command Line Arguments
 * ============================================ */

extern char **environ;

#endif /* _TACH_H_ */
