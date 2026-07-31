#include <kernel/string.h>
#include <proc/scheduler.h>

static struct process* g_runqueue[MAX_PROCESSES];
static size_t g_runqueue_count;
static size_t g_next_index;

void scheduler_init(void) {
    memset(g_runqueue, 0, sizeof(g_runqueue));
    g_runqueue_count = 0;
    g_next_index = 0;
    process_init();
}

void scheduler_add(struct process* proc) {
    if (!proc) {
        return;
    }
    for (size_t i = 0; i < g_runqueue_count; i++) {
        if (g_runqueue[i] == proc) {
            return;
        }
    }
    if (g_runqueue_count < MAX_PROCESSES) {
        g_runqueue[g_runqueue_count++] = proc;
    }
}

void scheduler_remove(struct process* proc) {
    for (size_t i = 0; i < g_runqueue_count; i++) {
        if (g_runqueue[i] != proc) {
            continue;
        }
        for (size_t j = i + 1; j < g_runqueue_count; j++) {
            g_runqueue[j - 1] = g_runqueue[j];
        }
        g_runqueue[--g_runqueue_count] = NULL;
        if (g_next_index >= g_runqueue_count) {
            g_next_index = 0;
        }
        return;
    }
}

struct process* scheduler_pick_next(void) {
    if (!g_runqueue_count) {
        return NULL;
    }
    for (size_t checked = 0; checked < g_runqueue_count; checked++) {
        struct process* proc = g_runqueue[g_next_index];
        g_next_index++;
        if (g_next_index >= g_runqueue_count) {
            g_next_index = 0;
        }
        if (proc && proc->state == PROCESS_STATE_RUNNING) {
            return proc;
        }
    }
    return NULL;
}

void scheduler_context_switch(struct process* from, struct process* to) {
    (void)from;
    if (to) {
        process_set_current(to);
    }
}

void scheduler_yield(void) {
    struct process* from = process_get_current();
    struct process* to = scheduler_pick_next();
    if (to == from && g_runqueue_count > 1) {
        struct process* alternative = scheduler_pick_next();
        if (alternative) {
            to = alternative;
        }
    }
    if (to && to != from) {
        scheduler_context_switch(from, to);
    }
}

void scheduler_tick(void) {
    struct process* current = process_get_current();
    if (current) {
        current->runtime_ticks++;
    }
    scheduler_yield();
}
