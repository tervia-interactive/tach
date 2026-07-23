/* tach - Kernel Log Header */
/*
 * dmesg-style boot/runtime logging. Every line is timestamped as
 * "[seconds.microseconds]" the way Linux's printk does, tagged with the
 * subsystem that emitted it. There is no real hardware timer wired up
 * yet (hal_timer_* are stubs), so klog advances its own monotonic
 * boot-time counter by a small amount on every call — enough to produce
 * realistic, strictly increasing timestamps during boot.
 */

#ifndef _KERNEL_KLOG_H
#define _KERNEL_KLOG_H

#include "kernel/types.h"

/* Resets the simulated boot-time clock to 0. Call once, first. */
void klog_init(void);

/* Advances the simulated boot-time clock by the given number of
 * microseconds. Useful for pacing a sequence of related log lines. */
void klog_advance(uint32_t usec);

/* Normal informational line: "[   0.001234] component: message" */
void klog_info(const char* component, const char* fmt, ...);

/* Warning line, prefixed "WARNING:" — a degraded but non-fatal condition. */
void klog_warn(const char* component, const char* fmt, ...);

/* Error line, prefixed "ERROR:" — a component failed to come up. */
void klog_err(const char* component, const char* fmt, ...);

/* Unadorned output with no timestamp/component prefix, for banners. */
void klog_raw(const char* fmt, ...);

#endif /* _KERNEL_KLOG_H */
