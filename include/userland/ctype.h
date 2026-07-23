/**
 * @file ctype.h
 * @brief Character type functions
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#ifndef _CTYPE_H_
#define _CTYPE_H_

int isalpha(int c);
int isdigit(int c);
int isalnum(int c);
int islower(int c);
int isupper(int c);
int isspace(int c);
int isprint(int c);
int tolower(int c);
int toupper(int c);

static inline int _is_alpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int _is_digit(int c) {
    return c >= '0' && c <= '9';
}

static inline int _is_lower(int c) {
    return c >= 'a' && c <= 'z';
}

static inline int _is_upper(int c) {
    return c >= 'A' && c <= 'Z';
}

static inline int _to_lower(int c) {
    if (_is_upper(c)) {
        return c - 'A' + 'a';
    }
    return c;
}

static inline int _to_upper(int c) {
    if (_is_lower(c)) {
        return c - 'a' + 'A';
    }
    return c;
}

#endif /* _CTYPE_H_ */
