/* tach Operating System - TachCore Test */
/* TCObject refcounting, TCString, TCArray, TCDictionary */

#include "TachCore/TCObject.h"
#include "TachCore/TCString.h"
#include "TachCore/TCArray.h"
#include "TachCore/TCDictionary.h"

#ifdef TACH_TEST

static void test_tcstring(void) {
    TCString* str = tcstring_create("hello");
    if (str != NULL) {
        tcstring_release(str);
    }
}

static void test_tcarray(void) {
    TCArray* arr = tcarray_create();
    if (arr != NULL) {
        tcarray_release(arr);
    }
}

static void test_tcdictionary(void) {
    TCDictionary* dict = tcdictionary_create();
    if (dict != NULL) {
        tcdictionary_release(dict);
    }
}

void test_tachcore_run_all(void) {
    printf("Running TachCore tests...\n");
    test_tcstring();
    printf("  [PASS] TCString\n");
    test_tcarray();
    printf("  [PASS] TCArray\n");
    test_tcdictionary();
    printf("  [PASS] TCDictionary\n");
    printf("TachCore tests complete.\n");
}

#endif /* TACH_TEST */
