/*
 * proc/scheduler.h - Scheduler (runqueues, context switch)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_SCHEDULER_H
#define _PROC_SCHEDULER_H

#include <kernel/types.h>
#include <proc/process.h>

#define MAX_RUNQUEUES 4
#define SCHEDULER_QUANTUM_MS 10

void scheduler_init(void);
void scheduler_add(struct process* proc);
void scheduler_remove(struct process* proc);
struct process* scheduler_pick_next(void);
void scheduler_yield(void);
void scheduler_tick(void);
void scheduler_context_switch(struct process* from, struct process* to);

#endif /* _PROC_SCHEDULER_H */
