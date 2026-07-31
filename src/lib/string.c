#include <kernel/string.h>

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0) {
        return dest;
    }
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

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

int memcmp(const void *lhs, const void *rhs, size_t n) {
    const unsigned char *a = (const unsigned char *)lhs;
    const unsigned char *b = (const unsigned char *)rhs;
    while (n--) {
        if (*a != *b) {
            return (int)*a - (int)*b;
        }
        a++;
        b++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

int strcmp(const char *lhs, const char *rhs) {
    while (*lhs && *lhs == *rhs) {
        lhs++;
        rhs++;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

int strncmp(const char *lhs, const char *rhs, size_t n) {
    while (n && *lhs && *lhs == *rhs) {
        lhs++;
        rhs++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

char *strcpy(char *dest, const char *src) {
    char *out = dest;
    while ((*dest++ = *src++) != '\0') {}
    return out;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *out = dest;
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    while (n--) {
        *dest++ = '\0';
    }
    return out;
}

const char *strchr(const char *str, int ch) {
    char needle = (char)ch;
    do {
        if (*str == needle) {
            return str;
        }
    } while (*str++);
    return NULL;
}
