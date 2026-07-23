/**
 * @file stdio.c
 * @brief Standard I/O implementation for tach
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "tach.h"
#include "stdio.h"

int getchar(void) {
    char c;
    ssize_t ret = read(STDIN_FILENO, &c, 1);
    if (ret < 0) {
        return EOF;
    }
    return (int)c;
}

char *gets(char *s) {
    int c;
    char *ptr = s;
    
    while ((c = getchar()) != EOF && c != '\n' && c != '\r') {
        *ptr++ = (char)c;
    }
    
    if (ptr == s && c == EOF) {
        return NULL;
    }
    
    *ptr = '\0';
    return s;
}
