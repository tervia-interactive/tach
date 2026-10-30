/* tach - Watchdog Header */
/* Supervised-process watchdog (used to keep tachd alive) */

#ifndef _KERNEL_WATCHDOG_H
#define _KERNEL_WATCHDOG_H

#include "kernel/types.h"

/* Watchdog handle: low 8 bits = slot + 1, upper 24 bits = generation, so a
 * stale handle from an unregistered entry never matches a reused slot. */
typedef uint32_t watchdog_handle_t;

#define WATCHDOG_INVALID 0u
#define WATCHDOG_MAX     8

/* Watchdog configuration */
typedef struct watchdog_config {
    uint32_t timeout_ms;
    uint32_t restart_count_max;
    bool auto_restart;
} watchdog_config_t;

/* Register process with watchdog. Returns WATCHDOG_INVALID if config is NULL,
 * timeout_ms is 0, or the table is full. The first deadline starts now. */
watchdog_handle_t watchdog_register(pid_t pid, const watchdog_config_t* config);

/* Unregister from watchdog */
void watchdog_unregister(watchdog_handle_t wd);

/* Feed watchdog (reset timer). Ignored once the entry has expired. */
void watchdog_feed(watchdog_handle_t wd);

/* True while the entry is registered and has not missed a deadline */
bool watchdog_is_alive(watchdog_handle_t wd);

/* On a missed deadline the process gets SIGKILL. If auto_restart is set and
 * fewer than restart_count_max restarts were used, the entry becomes
 * restart-pending; the supervisor respawns the process and calls
 * watchdog_rebind() with the new PID, which re-arms the deadline (0 on
 * success, -EINVAL bad handle, -EBUSY if no restart is pending). */
bool watchdog_restart_pending(watchdog_handle_t wd);
int watchdog_rebind(watchdog_handle_t wd, pid_t pid);

/* Number of successful rebinds so far */
uint32_t watchdog_restart_count(watchdog_handle_t wd);

/* Initialize watchdog subsystem */
void watchdog_init(void);

#endif /* _KERNEL_WATCHDOG_H */
