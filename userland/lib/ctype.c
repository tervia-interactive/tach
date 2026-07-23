/**
 * @file ctype.c
 * @brief Character type function implementations
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "ctype.h"

int isalpha(int c) {
    return _is_alpha(c);
}

int isdigit(int c) {
    return _is_digit(c);
}

int isalnum(int c) {
    return _is_alpha(c) || _is_digit(c);
}

int islower(int c) {
    return _is_lower(c);
}

int isupper(int c) {
    return _is_upper(c);
}

int isspace(int c) {
    return (c >= 9 && c <= 13) || c == ' ';
}

int isprint(int c) {
    return c >= 32 && c <= 126;
}

int tolower(int c) {
    return _to_lower(c);
}

int toupper(int c) {
    return _to_upper(c);
}
