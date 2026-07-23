/**
 * @file stdio.h
 * @brief Standard input/output functions
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)
#define BUFSIZ 1024

typedef struct {
    int fd;
    char *buffer;
    size_t pos;
    size_t size;
    int flags;
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);

int putchar(int c);
int puts(const char *s);
int getchar(void);
char *gets(char *s);

#endif /* _STDIO_H_ */
