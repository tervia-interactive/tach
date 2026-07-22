/* tach Operating System - Signal Test */
/* Signal delivery and default dispositions */

#include "kernel/types.h"
#include "kernel/assert.h"
#include "kernel/signal.h"

#ifdef TACH_TEST

static void test_signal_set_ops(void) {
    sigset_t set;
    
    sigemptyset(&set);
    KASSERT(!sigismember(&set, SIGTERM));
    
    sigaddset(&set, SIGTERM);
    KASSERT(sigismember(&set, SIGTERM));
    
    sigdelset(&set, SIGTERM);
    KASSERT(!sigismember(&set, SIGTERM));
    
    sigfillset(&set);
    KASSERT(sigismember(&set, SIGKILL));
}

void test_signal_run_all(void) {
    kprintf("Running signal tests...\n");
    test_signal_set_ops();
    kprintf("  [PASS] Signal set operations\n");
    kprintf("Signal tests complete.\n");
}

#endif /* TACH_TEST */
