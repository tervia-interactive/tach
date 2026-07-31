#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <fs/vfs.h>

#define PROCESS_STACK_PAGES 4

static struct process g_processes[MAX_PROCESSES];
static bool g_process_used[MAX_PROCESSES];
static struct process* g_current;
static pid_t g_next_pid;

#ifndef TACH_HOST_TEST
static void process_trampoline(void) {
    struct process* current = process_get_current();
    if (current && current->entry_point) {
        void (*entry)(void*) = (void (*)(void*))current->entry_point;
        entry(current->entry_arg);
    }
    process_exit(0);
    for (;;) {
        scheduler_yield();
    }
}

static uintptr_t prepare_initial_stack(void* stack, size_t bytes) {
    uintptr_t top = ((uintptr_t)stack + bytes) & ~(uintptr_t)0xf;
#if defined(__x86_64__)
    top -= 8;
    uintptr_t* frame = (uintptr_t*)(top - 7 * sizeof(uintptr_t));
    memset(frame, 0, 7 * sizeof(uintptr_t));
    frame[6] = (uintptr_t)process_trampoline;
    return (uintptr_t)frame;
#elif defined(__i386__)
    uintptr_t* frame = (uintptr_t*)(top - 5 * sizeof(uintptr_t));
    memset(frame, 0, 5 * sizeof(uintptr_t));
    frame[4] = (uintptr_t)process_trampoline;
    return (uintptr_t)frame;
#elif defined(__aarch64__)
    uintptr_t* frame = (uintptr_t*)(top - 12 * sizeof(uintptr_t));
    memset(frame, 0, 12 * sizeof(uintptr_t));
    frame[1] = (uintptr_t)process_trampoline;
    return (uintptr_t)frame;
#elif defined(__arm__)
    uintptr_t* frame = (uintptr_t*)(top - 10 * sizeof(uintptr_t));
    memset(frame, 0, 10 * sizeof(uintptr_t));
    frame[9] = (uintptr_t)process_trampoline;
    return (uintptr_t)frame;
#elif defined(__riscv) && __riscv_xlen == 64
    uintptr_t* frame = (uintptr_t*)(top - 14 * sizeof(uintptr_t));
    memset(frame, 0, 14 * sizeof(uintptr_t));
    frame[0] = (uintptr_t)process_trampoline;
    return (uintptr_t)frame;
#elif defined(__riscv)
    uintptr_t* frame = (uintptr_t*)(top - 16 * sizeof(uintptr_t));
    memset(frame, 0, 16 * sizeof(uintptr_t));
    frame[0] = (uintptr_t)process_trampoline;
    return (uintptr_t)frame;
#else
    return top;
#endif
}
#endif

void process_init(void) {
    memset(g_processes, 0, sizeof(g_processes));
    memset(g_process_used, 0, sizeof(g_process_used));
    g_current = NULL;
    g_next_pid = 1;
}

static void process_attach_stdio(struct process* proc) {
    struct vnode* console = vfs_console();
    if (!proc || !proc->fds || !console) return;
    (void)fd_alloc(proc->fds, console, O_RDONLY);
    (void)fd_alloc(proc->fds, console, O_WRONLY);
    (void)fd_alloc(proc->fds, console, O_WRONLY);
}

struct process* process_create(const char* name) {
    if (!name || !*name) return NULL;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_used[i]) continue;
        struct fd_table* fds = fd_table_create();
        if (!fds) return NULL;

#ifndef TACH_HOST_TEST
        struct vmm_context* address_space = vmm_create_context();
        if (!address_space) {
            fd_table_destroy(fds);
            return NULL;
        }
#endif
        struct process* proc = &g_processes[i];
        memset(proc, 0, sizeof(*proc));
        g_process_used[i] = true;
        proc->pid = g_next_pid++;
        proc->ppid = g_current ? g_current->pid : 0;
        strncpy(proc->name, name, MAX_PROCESS_NAME - 1);
        proc->state = PROCESS_STATE_STOPPED;
        proc->fds = fds;
#ifndef TACH_HOST_TEST
        proc->mm = address_space;
        proc->mm->owner = proc->pid;
#endif
        proc->uid = 0;
        proc->gid = 0;
        proc->refcount = 1;
        process_attach_stdio(proc);
        return proc;
    }
    return NULL;
}

void process_destroy(struct process* proc) {
    if (!proc) return;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (&g_processes[i] != proc || !g_process_used[i]) continue;
        scheduler_remove(proc);
        fd_table_destroy(proc->fds);
#ifndef TACH_HOST_TEST
        if (proc->stack && proc->stack_pages) {
            pmm_free_pages(proc->stack, proc->stack_pages);
        }
        if (proc->mm) {
            vmm_destroy_context(proc->mm);
        }
#endif
        if (g_current == proc) g_current = NULL;
        memset(proc, 0, sizeof(*proc));
        g_process_used[i] = false;
        return;
    }
}

struct process* process_get(pid_t pid) {
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_used[i] && g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }
    return NULL;
}

struct process* process_get_current(void) { return g_current; }
void process_set_current(struct process* proc) { g_current = proc; }

int process_start(struct process* proc, void* entry, void* arg) {
    if (!proc || !entry) return -EINVAL;
    proc->entry_point = entry;
    proc->entry_arg = arg;
#ifndef TACH_HOST_TEST
    proc->stack = pmm_alloc_pages(PROCESS_STACK_PAGES);
    if (!proc->stack) return -ENOMEM;
    proc->stack_pages = PROCESS_STACK_PAGES;
    proc->saved_stack = prepare_initial_stack(
        proc->stack, PROCESS_STACK_PAGES * PAGE_SIZE);
#endif
    proc->state = PROCESS_STATE_RUNNING;
    scheduler_add(proc);
    return 0;
}

void process_exit(int code) {
    if (!g_current) return;
    g_current->exit_code = code;
    g_current->state = PROCESS_STATE_ZOMBIE;
    scheduler_remove(g_current);
#ifndef TACH_HOST_TEST
    scheduler_yield();
    for (;;) {}
#endif
}

void process_wait(struct process* proc) {
    if (!proc) return;
    while (proc->state != PROCESS_STATE_ZOMBIE) scheduler_yield();
}

size_t process_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_used[i]) count++;
    }
    return count;
}

struct process* process_at(size_t index) {
    size_t current = 0;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (!g_process_used[i]) continue;
        if (current++ == index) return &g_processes[i];
    }
    return NULL;
}

const char* process_state_name(int state) {
    switch (state) {
        case PROCESS_STATE_RUNNING: return "running";
        case PROCESS_STATE_SLEEPING: return "sleeping";
        case PROCESS_STATE_STOPPED: return "stopped";
        case PROCESS_STATE_ZOMBIE: return "zombie";
        case PROCESS_STATE_BLOCKED: return "blocked";
        default: return "unused";
    }
}
