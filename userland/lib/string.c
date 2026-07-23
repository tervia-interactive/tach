/**
 * @file string.c
 * @brief String manipulation function implementations
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "string.h"

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    
    while (n--) {
        *p++ = (unsigned char)c;
    }
    
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    
    while ((*d++ = *src++) != '\0');
    
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    
    while (n && (*d++ = *src++) != '\0') {
        n--;
    }
    
    while (n--) {
        *d++ = '\0';
    }
    
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    
    while (*d) {
        d++;
    }
    
    while ((*d++ = *src++) != '\0');
    
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    
    while (*d) {
        d++;
    }
    
    while (n-- && (*d++ = *src++) != '\0');
    
    if (d > dest && *(d-1) != '\0') {
        *d = '\0';
    }
    
    return dest;
}

size_t strlen(const char *s) {
    size_t len = 0;
    
    while (*s++) {
        len++;
    }
    
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) {
        return 0;
    }
    
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strchr(const char *s, int c) {
    while (*s && *s != (char)c) {
        s++;
    }
    
    return (*s == (char)c) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    
    if ((char)c == '\0') {
        return (char *)s;
    }
    
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    
    if (needle_len == 0) {
        return (char *)haystack;
    }
    
    while (*haystack) {
        if (*haystack == *needle) {
            if (strncmp(haystack, needle, needle_len) == 0) {
                return (char *)haystack;
            }
        }
        haystack++;
    }
    
    return NULL;
}
