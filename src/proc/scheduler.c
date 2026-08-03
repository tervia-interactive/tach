#include <kernel/string.h>
#include <hal/irq.h>
#include <hal/smp.h>
#include <kernel/spinlock.h>
#include <kernel/percpu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/scheduler.h>

struct cpu_runqueue {
    spinlock_t lock;
    struct process* processes[MAX_PROCESSES];
    size_t count;
    size_t next;
    uintptr_t idle_stack;
};
static struct cpu_runqueue g_runqueues[MAX_CPUS];
static spinlock_t g_balance_lock;

#ifndef TACH_HOST_TEST
extern void task_switch(uintptr_t* old_stack, uintptr_t* new_stack);
#endif

void scheduler_init(void) {
    memset(g_runqueues, 0, sizeof(g_runqueues));
    spinlock_init(&g_balance_lock);
    for (size_t cpu = 0; cpu < MAX_CPUS; cpu++)
        spinlock_init(&g_runqueues[cpu].lock);
    process_init();
}

void scheduler_init_cpu(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) return;
    process_set_current(NULL);
}

void scheduler_add(struct process* proc) {
    if (!proc || proc->queued) return;
    uint32_t target = 0;
    size_t lightest = (size_t)-1;
    spinlock_lock(&g_balance_lock);
    int cpus = hal_smp_cpu_count();
    for (int cpu = 0; cpu < cpus; cpu++) {
        if (g_runqueues[cpu].count < lightest) {
            target = (uint32_t)cpu;
            lightest = g_runqueues[cpu].count;
        }
    }
    struct cpu_runqueue* queue = &g_runqueues[target];
    spinlock_lock(&queue->lock);
    for (size_t i = 0; i < queue->count; i++) {
        if (queue->processes[i] == proc) {
            spinlock_unlock(&queue->lock);
            spinlock_unlock(&g_balance_lock);
            return;
        }
    }
    if (queue->count < MAX_PROCESSES) {
        queue->processes[queue->count++] = proc;
        proc->cpu_id = target;
        proc->queued = true;
    }
    spinlock_unlock(&queue->lock);
    spinlock_unlock(&g_balance_lock);
}

void scheduler_remove(struct process* proc) {
    uint32_t cpu = proc && proc->cpu_id < MAX_CPUS ? proc->cpu_id : 0;
    struct cpu_runqueue* queue = &g_runqueues[cpu];
    spinlock_lock(&queue->lock);
    for (size_t i = 0; i < queue->count; i++) {
        if (queue->processes[i] != proc) continue;
        for (size_t j = i + 1; j < queue->count; j++) {
            queue->processes[j - 1] = queue->processes[j];
        }
        queue->processes[--queue->count] = NULL;
        proc->queued = false;
        if (queue->next >= queue->count) queue->next = 0;
        spinlock_unlock(&queue->lock);
        return;
    }
    spinlock_unlock(&queue->lock);
}

struct process* scheduler_pick_next(void) {
    struct cpu_runqueue* queue = &g_runqueues[hal_smp_current_cpu()];
    spinlock_lock(&queue->lock);
    if (!queue->count) {
        spinlock_unlock(&queue->lock);
        return NULL;
    }
    for (size_t checked = 0; checked < queue->count; checked++) {
        struct process* proc = queue->processes[queue->next++];
        if (queue->next >= queue->count) queue->next = 0;
        if (proc && proc->state == PROCESS_STATE_RUNNING && !proc->on_cpu) {
            spinlock_unlock(&queue->lock);
            return proc;
        }
    }
    spinlock_unlock(&queue->lock);
    return NULL;
}

void scheduler_context_switch(struct process* from, struct process* to) {
    if (!to || to == from) return;
    if (from) from->on_cpu = false;
    process_set_current(to);
#ifndef TACH_HOST_TEST
    vmm_switch_context(to->mm);
    if (to->stack && to->stack_pages) {
        arch_set_kernel_stack((uintptr_t)to->stack +
                              to->stack_pages * PAGE_SIZE);
    }
    uintptr_t* old_stack = from ? &from->saved_stack :
        &g_runqueues[hal_smp_current_cpu()].idle_stack;
    task_switch(old_stack, &to->saved_stack);
#else
    (void)from;
#endif
}

static void scheduler_switch_to_idle(struct process* from) {
    if (!from) return;
    uint32_t cpu = hal_smp_current_cpu();
    from->on_cpu = false;
    process_set_current(NULL);
#ifndef TACH_HOST_TEST
    vmm_switch_context(vmm_kernel_context());
    task_switch(&from->saved_stack, &g_runqueues[cpu].idle_stack);
#else
    (void)cpu;
#endif
}

void scheduler_yield(void) {
    struct process* from = process_get_current();
    struct process* to = scheduler_pick_next();
    if (!to) {
        if (from && from->state != PROCESS_STATE_RUNNING)
            scheduler_switch_to_idle(from);
        return;
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
