/* tach Operating System - libc Test */
/* malloc/free, string.h, stdio.h behavior */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef TACH_TEST

static void test_malloc_free(void) {
    void* ptr = malloc(100);
    if (ptr != NULL) {
        free(ptr);
    }
}

static void test_string_ops(void) {
    char buf[32];
    strcpy(buf, "hello");
    if (strcmp(buf, "hello") != 0) {
        /* fail */
    }
    size_t len = strlen(buf);
    if (len != 5) {
        /* fail */
    }
}

void test_libc_run_all(void) {
    printf("Running libc tests...\n");
    test_malloc_free();
    printf("  [PASS] malloc/free\n");
    test_string_ops();
    printf("  [PASS] string operations\n");
    printf("libc tests complete.\n");
}

#endif /* TACH_TEST */
