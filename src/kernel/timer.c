/*
 * tach - Kernel timer wheel
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Delayed-work timers driven by the boot CPU's periodic tick (100 Hz).
 * Timers live in caller-provided storage (no allocation), kept in a list
 * sorted by expiry; equal expiries fire in the order they were armed.
 *
 * Callbacks run in timer-interrupt context on the boot CPU with the timer
 * lock released, so a callback may re-arm or cancel its own timer.
 */

#include <kernel/types.h>
#include <kernel/timer.h>
#include <kernel/spinlock.h>
#include <hal/time.h>

static spinlock_t g_timer_lock = SPINLOCK_INIT;
static timer_t* g_timer_head;

/* Caller holds g_timer_lock. */
static void timer_unlink_locked(timer_t* tmr) {
    timer_t** link = &g_timer_head;
    while (*link) {
        if (*link == tmr) {
            *link = tmr->next;
            break;
        }
        link = &(*link)->next;
    }
    tmr->next = NULL;
    tmr->active = false;
}

void timer_subsystem_init(void) {
    spinlock_init(&g_timer_lock);
    g_timer_head = NULL;
}

void timer_init(timer_t* tmr) {
    if (!tmr) return;
    tmr->expires = 0;
    tmr->callback = NULL;
    tmr->arg = NULL;
    tmr->next = NULL;
    tmr->active = false;
}

tick_t timer_get_ticks(void) {
    return (tick_t)hal_timer_get_ticks();
}

void timer_set(timer_t* tmr, tick_t ticks, timer_callback_t cb, void* arg) {
    if (!tmr || !cb) return;
    if (ticks == 0) ticks = 1;

    spinlock_lock_irq(&g_timer_lock);
    if (tmr->active) timer_unlink_locked(tmr);

    tmr->expires = timer_get_ticks() + ticks;
    tmr->callback = cb;
    tmr->arg = arg;

    timer_t** link = &g_timer_head;
    while (*link && (*link)->expires <= tmr->expires) link = &(*link)->next;
    tmr->next = *link;
    *link = tmr;
    tmr->active = true;
    spinlock_unlock_irq(&g_timer_lock);
}

void timer_cancel(timer_t* tmr) {
    if (!tmr) return;
    spinlock_lock_irq(&g_timer_lock);
    if (tmr->active) timer_unlink_locked(tmr);
    spinlock_unlock_irq(&g_timer_lock);
}

bool timer_is_active(timer_t* tmr) {
    if (!tmr) return false;
    spinlock_lock_irq(&g_timer_lock);
    bool active = tmr->active;
    spinlock_unlock_irq(&g_timer_lock);
    return active;
}

void timer_tick(void) {
    for (;;) {
        spinlock_lock_irq(&g_timer_lock);
        timer_t* tmr = g_timer_head;
        if (!tmr || tmr->expires > timer_get_ticks()) {
            spinlock_unlock_irq(&g_timer_lock);
            return;
        }
        g_timer_head = tmr->next;
        tmr->next = NULL;
        tmr->active = false;
        timer_callback_t callback = tmr->callback;
        void* arg = tmr->arg;
        spinlock_unlock_irq(&g_timer_lock);

        callback(arg);
    }
}
