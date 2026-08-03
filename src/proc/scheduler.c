#include <kernel/string.h>
#include <hal/irq.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/scheduler.h>

static struct process* g_runqueue[MAX_PROCESSES];
static size_t g_runqueue_count;
static size_t g_next_index;

#ifndef TACH_HOST_TEST
extern void task_switch(uintptr_t* old_stack, uintptr_t* new_stack);
#endif

void scheduler_init(void) {
    memset(g_runqueue, 0, sizeof(g_runqueue));
    g_runqueue_count = 0;
    g_next_index = 0;
    process_init();
}

void scheduler_add(struct process* proc) {
    if (!proc) return;
    irq_flags_t flags = hal_irq_save();
    for (size_t i = 0; i < g_runqueue_count; i++) {
        if (g_runqueue[i] == proc) {
            hal_irq_restore(flags);
            return;
        }
    }
    if (g_runqueue_count < MAX_PROCESSES) {
        g_runqueue[g_runqueue_count++] = proc;
    }
    hal_irq_restore(flags);
}

void scheduler_remove(struct process* proc) {
    irq_flags_t flags = hal_irq_save();
    for (size_t i = 0; i < g_runqueue_count; i++) {
        if (g_runqueue[i] != proc) continue;
        for (size_t j = i + 1; j < g_runqueue_count; j++) {
            g_runqueue[j - 1] = g_runqueue[j];
        }
        g_runqueue[--g_runqueue_count] = NULL;
        if (g_next_index >= g_runqueue_count) g_next_index = 0;
        hal_irq_restore(flags);
        return;
    }
    hal_irq_restore(flags);
}

struct process* scheduler_pick_next(void) {
    irq_flags_t flags = hal_irq_save();
    if (!g_runqueue_count) {
        hal_irq_restore(flags);
        return NULL;
    }
    for (size_t checked = 0; checked < g_runqueue_count; checked++) {
        struct process* proc = g_runqueue[g_next_index++];
        if (g_next_index >= g_runqueue_count) g_next_index = 0;
        if (proc && proc->state == PROCESS_STATE_RUNNING) {
            hal_irq_restore(flags);
            return proc;
        }
    }
    hal_irq_restore(flags);
    return NULL;
}

void scheduler_context_switch(struct process* from, struct process* to) {
    if (!to || to == from) return;
    process_set_current(to);
#ifndef TACH_HOST_TEST
    vmm_switch_context(to->mm);
    if (to->stack && to->stack_pages) {
        arch_set_kernel_stack((uintptr_t)to->stack +
                              to->stack_pages * PAGE_SIZE);
    }
    if (from) {
        task_switch(&from->saved_stack, &to->saved_stack);
    }
#else
    (void)from;
#endif
}

void scheduler_yield(void) {
    struct process* from = process_get_current();
    struct process* to = scheduler_pick_next();
    if (to == from && g_runqueue_count > 1) {
        struct process* alternative = scheduler_pick_next();
        if (alternative) to = alternative;
    }
    if (to && to != from) scheduler_context_switch(from, to);
}

void scheduler_block_current(void* reason) {
    struct process* current = process_get_current();
    if (!current) return;
    current->block_reason = reason;
    current->state = PROCESS_STATE_BLOCKED;
    scheduler_yield();
}

void scheduler_wake(struct process* proc) {
    if (!proc || proc->state != PROCESS_STATE_BLOCKED) return;
    proc->block_reason = NULL;
    proc->state = PROCESS_STATE_RUNNING;
    scheduler_add(proc);
}

void scheduler_tick(void) {
    struct process* current = process_get_current();
    if (current) current->runtime_ticks++;
    scheduler_yield();
}
