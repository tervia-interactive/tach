/* tach Operating System - PMM Test */
/* Physical memory manager correctness (alloc/free/fragmentation) */

#include "kernel/types.h"
#include "kernel/assert.h"
#include "mm/pmm.h"

#ifdef TACH_TEST

static void test_pmm_basic(void) {
    /* Test basic allocation and free */
    uintptr_t frame = pmm_alloc_frame();
    KASSERT(frame != 0);
    pmm_free_frame(frame);
}

static void test_pmm_multiple(void) {
    /* Test multiple allocations */
    uintptr_t frames[10];
    for (int i = 0; i < 10; i++) {
        frames[i] = pmm_alloc_frame();
        KASSERT(frames[i] != 0);
    }
    for (int i = 0; i < 10; i++) {
        pmm_free_frame(frames[i]);
    }
}

static void test_pmm_fragmentation(void) {
    /* Test fragmentation handling */
    uintptr_t frames[100];
    int allocated = 0;
    
    /* Allocate many frames */
    for (int i = 0; i < 100; i++) {
        frames[i] = pmm_alloc_frame();
        if (frames[i] != 0) allocated++;
    }
    
    /* Free every other frame */
    for (int i = 0; i < allocated; i += 2) {
        pmm_free_frame(frames[i]);
    }
    
    /* Allocate again - should succeed */
    uintptr_t new_frame = pmm_alloc_frame();
    KASSERT(new_frame != 0);
    pmm_free_frame(new_frame);
}

void test_pmm_run_all(void) {
    kprintf("Running PMM tests...\n");
    test_pmm_basic();
    kprintf("  [PASS] Basic alloc/free\n");
    test_pmm_multiple();
    kprintf("  [PASS] Multiple allocations\n");
    test_pmm_fragmentation();
    kprintf("  [PASS] Fragmentation handling\n");
    kprintf("PMM tests complete.\n");
}

#endif /* TACH_TEST */
