/*
 * tach - Supervised-process watchdog
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * A supervised process must call watchdog_feed() at least once per
 * timeout_ms. If it does not, the watchdog sends it SIGKILL and, when
 * auto_restart is set and the restart budget is not exhausted, marks the
 * entry restart-pending so the supervisor can respawn it and call
 * watchdog_rebind(). Once the budget is spent the entry is failed for good.
 *
 * Expiry runs from the timer wheel (interrupt context); every lock taken
 * here is IRQ-safe and the lock order is watchdog -> timer, never the
 * reverse (timer callbacks run with the timer lock released).
 */

#include <kernel/types.h>
#include <kernel/watchdog.h>
#include <kernel/timer.h>
#include <kernel/spinlock.h>
#include <kernel/signal.h>
#include <kernel/errno.h>
#include <kernel/klog.h>

/* The system tick is 100 Hz; keep the conversion in 32-bit constant
 * division so 32-bit targets do not need libgcc 64-bit helpers. */
#define WATCHDOG_TICK_MS 10u

enum {
    WD_FREE = 0,
    WD_ARMED,
    WD_RESTART,   /* killed, restart budget left: waiting for rebind */
    WD_FAILED     /* killed, no restarts left (or auto_restart off)  */
};

struct wd_entry {
    uint32_t generation;          /* 24-bit, bumped on every registration */
    int state;
    pid_t pid;
    watchdog_config_t config;
    timer_t timer;
    uint32_t restarts;
};

static struct wd_entry g_wd[WATCHDOG_MAX];
static spinlock_t g_wd_lock = SPINLOCK_INIT;

static tick_t wd_ms_to_ticks(uint32_t ms) {
    uint32_t ticks = ms / WATCHDOG_TICK_MS;
    if (ms % WATCHDOG_TICK_MS) ticks++;
    return ticks ? (tick_t)ticks : (tick_t)1;
}

/* Caller holds g_wd_lock. */
static struct wd_entry* wd_lookup_locked(watchdog_handle_t wd) {
    uint32_t slot = wd & 0xffu;
    if (slot == 0 || slot > WATCHDOG_MAX) return NULL;
    struct wd_entry* entry = &g_wd[slot - 1];
    if (entry->state == WD_FREE) return NULL;
    if (entry->generation != (wd >> 8)) return NULL;
    return entry;
}

static void wd_expire(void* arg);

/* Caller holds g_wd_lock. */
static void wd_arm_locked(struct wd_entry* entry) {
    timer_set(&entry->timer, wd_ms_to_ticks(entry->config.timeout_ms),
              wd_expire, entry);
}

static void wd_expire(void* arg) {
    struct wd_entry* entry = (struct wd_entry*)arg;

    spinlock_lock_irq(&g_wd_lock);
    /* A feed or unregister may have raced with this callback after it was
     * dequeued; a re-armed timer has a deadline still in the future. */
    if (entry->state != WD_ARMED || timer_get_ticks() < entry->timer.expires) {
        spinlock_unlock_irq(&g_wd_lock);
        return;
    }
    pid_t victim = entry->pid;
    uint32_t timeout_ms = entry->config.timeout_ms;
    bool restart = entry->config.auto_restart &&
                   entry->restarts < entry->config.restart_count_max;
    entry->state = restart ? WD_RESTART : WD_FAILED;
    spinlock_unlock_irq(&g_wd_lock);

    klog_err("watchdog", "pid %u missed its %u ms deadline, killing (%s)",
             (unsigned)victim, (unsigned)timeout_ms,
             restart ? "restart pending" : "no restarts left");
    (void)signal_send(victim, SIGKILL);
}

void watchdog_init(void) {
    spinlock_lock_irq(&g_wd_lock);
    for (size_t i = 0; i < WATCHDOG_MAX; i++) {
        if (g_wd[i].state != WD_FREE) timer_cancel(&g_wd[i].timer);
        g_wd[i].state = WD_FREE;
    }
    spinlock_unlock_irq(&g_wd_lock);
}

watchdog_handle_t watchdog_register(pid_t pid, const watchdog_config_t* config) {
    if (!config || config->timeout_ms == 0) return WATCHDOG_INVALID;

    watchdog_handle_t handle = WATCHDOG_INVALID;
    spinlock_lock_irq(&g_wd_lock);
    for (size_t i = 0; i < WATCHDOG_MAX; i++) {
        struct wd_entry* entry = &g_wd[i];
        if (entry->state != WD_FREE) continue;

        entry->generation = (entry->generation + 1u) & 0xffffffu;
        if (entry->generation == 0) entry->generation = 1;
        entry->pid = pid;
        entry->config = *config;
        entry->restarts = 0;
        entry->state = WD_ARMED;
        timer_init(&entry->timer);
        wd_arm_locked(entry);
        handle = (entry->generation << 8) | (uint32_t)(i + 1);
        break;
    }
    spinlock_unlock_irq(&g_wd_lock);
    return handle;
}

void watchdog_unregister(watchdog_handle_t wd) {
    spinlock_lock_irq(&g_wd_lock);
    struct wd_entry* entry = wd_lookup_locked(wd);
    if (entry) {
        timer_cancel(&entry->timer);
        entry->state = WD_FREE;
    }
    spinlock_unlock_irq(&g_wd_lock);
}

void watchdog_feed(watchdog_handle_t wd) {
    spinlock_lock_irq(&g_wd_lock);
    struct wd_entry* entry = wd_lookup_locked(wd);
    if (entry && entry->state == WD_ARMED) wd_arm_locked(entry);
    spinlock_unlock_irq(&g_wd_lock);
}

bool watchdog_is_alive(watchdog_handle_t wd) {
    spinlock_lock_irq(&g_wd_lock);
    struct wd_entry* entry = wd_lookup_locked(wd);
    bool alive = entry && entry->state == WD_ARMED;
    spinlock_unlock_irq(&g_wd_lock);
    return alive;
}

bool watchdog_restart_pending(watchdog_handle_t wd) {
    spinlock_lock_irq(&g_wd_lock);
    struct wd_entry* entry = wd_lookup_locked(wd);
    bool pending = entry && entry->state == WD_RESTART;
    spinlock_unlock_irq(&g_wd_lock);
    return pending;
}

uint32_t watchdog_restart_count(watchdog_handle_t wd) {
    spinlock_lock_irq(&g_wd_lock);
    struct wd_entry* entry = wd_lookup_locked(wd);
    uint32_t restarts = entry ? entry->restarts : 0;
    spinlock_unlock_irq(&g_wd_lock);
    return restarts;
}

int watchdog_rebind(watchdog_handle_t wd, pid_t pid) {
    int result;
    spinlock_lock_irq(&g_wd_lock);
    struct wd_entry* entry = wd_lookup_locked(wd);
    if (!entry) {
        result = -EINVAL;
    } else if (entry->state != WD_RESTART) {
        result = -EBUSY;
    } else {
        entry->pid = pid;
        entry->restarts++;
        entry->state = WD_ARMED;
        wd_arm_locked(entry);
        result = 0;
    }
    spinlock_unlock_irq(&g_wd_lock);
    return result;
}
