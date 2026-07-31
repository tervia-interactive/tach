/*
 * kernel/string.h - Freestanding string and memory helpers
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _KERNEL_STRING_H
#define _KERNEL_STRING_H

#include <kernel/types.h>

void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* dest, int value, size_t n);
int memcmp(const void* lhs, const void* rhs, size_t n);

size_t strlen(const char* str);
int strcmp(const char* lhs, const char* rhs);
int strncmp(const char* lhs, const char* rhs, size_t n);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
const char* strchr(const char* str, int ch);

#endif /* _KERNEL_STRING_H */
