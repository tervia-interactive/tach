/*
 * userland/runtime.c - Embedded PID 1 and shell fallback bootstrap
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#include <kernel/errno.h>
#include <kernel/klog.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <term/shell.h>
#include <userland/runtime.h>
#include <fs/vfs.h>

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

    const void* init_image;
    size_t init_size;
#ifndef TACH_HOST_TEST
    if (arch_user_supported() &&
        vfs_read_file("/sbin/init", &init_image, &init_size) == 0) {
        struct process* external_init = process_create("init");
        if (!external_init) return -ENOMEM;
        int loaded = process_exec_image(external_init, init_image, init_size,
                                        "/sbin/init");
        if (loaded < 0) {
            process_destroy(external_init);
            klog_err("init", "cannot execute /sbin/init (%d)", loaded);
            return loaded;
        }
        klog_info("init", "PID 1 executing /sbin/init in userspace");
        while (external_init->state != PROCESS_STATE_ZOMBIE)
            scheduler_yield();
        int status = external_init->exit_code;
        process_destroy(external_init);
        return status;
    }
#else
    (void)init_image; (void)init_size;
#endif

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
    klog_info("init", "starting shell as PID %u",
              (unsigned)shell->pid);

#ifdef TACH_HOST_TEST
    process_set_current(shell);
    shell_entry(tty);
    process_set_current(init);
#endif

    /* PID 1 must wait for and reap its shell.  A single yield is not a
     * completion barrier: once timer preemption is enabled, the scheduler
     * can return to init on the very next tick while the shell is still
     * running. */
    int status = 0;
    pid_t waited = process_waitpid(shell->pid, &status, 0);
    if ((int32_t)waited < 0) {
        return (int32_t)waited;
    }
    return status;
}
