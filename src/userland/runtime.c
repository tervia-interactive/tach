/*
 * userland/runtime.c - Embedded PID 1 and interactive userspace bootstrap
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#include <kernel/errno.h>
#include <kernel/klog.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <term/shell.h>
#include <userland/runtime.h>

static void init_entry(void* arg) {
    (void)arg;
}

static void shell_entry(void* arg) {
    tty_t* tty = (tty_t*)arg;
    shell_t shell;
    shell_init(&shell, "tach");
    shell.tty = tty;
    shell_run(&shell);
}

int userland_bootstrap(tty_t* tty) {
    if (!tty) {
        return -EINVAL;
    }

    struct process* init = process_create("init");
    if (!init) {
        return -ENOMEM;
    }
    if (process_start(init, (void*)init_entry, NULL) < 0) {
        process_destroy(init);
        return -EIO;
    }
    process_set_current(init);

    struct process* shell = process_create("sh");
    if (!shell) {
        return -ENOMEM;
    }
    if (process_start(shell, (void*)shell_entry, tty) < 0) {
        process_destroy(shell);
        return -EIO;
    }

    klog_info("init", "PID 1 started embedded userspace runtime");
    klog_info("init", "starting interactive shell as PID %u",
              (unsigned)shell->pid);

#ifdef TACH_HOST_TEST
    process_set_current(shell);
    shell_entry(tty);
    process_set_current(init);
#else
    scheduler_yield();
#endif
    int status = shell->exit_code;
    return status;
}
