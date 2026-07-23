/* tach - Watchdog Header */
/* Supervised-process watchdog (used to keep tachd alive) */

#ifndef _KERNEL_WATCHDOG_H
#define _KERNEL_WATCHDOG_H

#include "kernel/types.h"

/* Watchdog handle */
typedef uint32_t watchdog_handle_t;

/* Watchdog configuration */
typedef struct watchdog_config {
    uint32_t timeout_ms;
    uint32_t restart_count_max;
    bool auto_restart;
} watchdog_config_t;

/* Register process with watchdog */
watchdog_handle_t watchdog_register(pid_t pid, const watchdog_config_t* config);

/* Unregister from watchdog */
void watchdog_unregister(watchdog_handle_t wd);

/* Feed watchdog (reset timer) */
void watchdog_feed(watchdog_handle_t wd);

/* Get watchdog status */
bool watchdog_is_alive(watchdog_handle_t wd);

/* Initialize watchdog subsystem */
void watchdog_init(void);

#endif /* _KERNEL_WATCHDOG_H */
