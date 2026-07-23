/**
 * @file stdlib.h
 * @brief General utilities: memory allocation, process control, conversions
 * 
 * @copyright Copyright 2026 Tervia Interactive™
 * @license Apache License, Version 2.0
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define RAND_MAX 32767

typedef struct {
    int quot;
    int rem;
} div_t;

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

_Noreturn void exit(int status);
int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);

int abs(int j);
div_t div(int numer, int denom);

int rand(void);
void srand(unsigned int seed);

#endif /* _STDLIB_H_ */
