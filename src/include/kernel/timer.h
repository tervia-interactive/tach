/* tach - Timer Header */
/* Kernel timer wheel — timeouts, delayed work */

#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include "kernel/types.h"

/* Timer callback type */
typedef void (*timer_callback_t)(void* arg);

/* Timer structure */
typedef struct timer {
    tick_t expires;
    timer_callback_t callback;
    void* arg;
    struct timer* next;
    bool active;
} timer_t;

/* Initialize timer */
void timer_init(timer_t* tmr);

/* Set timer */
void timer_set(timer_t* tmr, tick_t ticks, timer_callback_t cb, void* arg);

/* Cancel timer */
void timer_cancel(timer_t* tmr);

/* Check if timer is active */
bool timer_is_active(timer_t* tmr);

/* Get current tick count */
tick_t timer_get_ticks(void);

/* Initialize timer subsystem */
void timer_subsystem_init(void);

#endif /* _KERNEL_TIMER_H */
