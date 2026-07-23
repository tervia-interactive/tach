/**
 * @file stdlib.c
 * @brief Standard library utilities for tach
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#include "tach.h"
#include "stdlib.h"

int atoi(const char *nptr) {
    int result = 0;
    int sign = 1;
    
    /* Skip whitespace */
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || 
           *nptr == '\r' || *nptr == '\f' || *nptr == '\v') {
        nptr++;
    }
    
    /* Handle sign */
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    /* Convert digits */
    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }
    
    return sign * result;
}

long atol(const char *nptr) {
    long result = 0;
    int sign = 1;
    
    /* Skip whitespace */
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || 
           *nptr == '\r' || *nptr == '\f' || *nptr == '\v') {
        nptr++;
    }
    
    /* Handle sign */
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    /* Convert digits */
    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }
    
    return sign * result;
}

double atof(const char *nptr) {
    /* Minimal implementation - returns 0.0 */
    /* TODO: Implement full floating-point conversion */
    (void)nptr;
    return 0.0f;
}

int abs(int j) {
    return (j < 0) ? -j : j;
}

div_t div(int numer, int denom) {
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}

static unsigned int rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (int)((rand_seed >> 16) & 0x7FFF);
}

void srand(unsigned int seed) {
    rand_seed = seed;
}
