#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/elf.h>
#include <proc/wait.h>
#include <kernel/signal.h>
#include <fs/vfs.h>
#include <hal/smp.h>
#include <kernel/percpu.h>
#include <kernel/spinlock.h>

#define PROCESS_STACK_PAGES 4

static struct process g_processes[MAX_PROCESSES];
static bool g_process_used[MAX_PROCESSES];
static struct process* g_current[MAX_CPUS];
static pid_t g_next_pid;
static spinlock_t g_process_lock;

#ifndef TACH_HOST_TEST
static void process_trampoline(void) {
    struct process* current = process_get_current();
    if (current && current->user_mode) {
        struct user_context context = current->user_context;
        (void)signal_deliver_pending(current, &context);
        current->user_context = context;
        arch_enter_user(&current->user_context);
    }
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

static int allocate_kernel_stack(struct process* process) {
    if (process->stack) return 0;
    process->stack = pmm_alloc_pages(PROCESS_STACK_PAGES);
    if (!process->stack) return -ENOMEM;
    process->stack_pages = PROCESS_STACK_PAGES;
    process->saved_stack = prepare_initial_stack(
        process->stack, PROCESS_STACK_PAGES * PAGE_SIZE);
    return 0;
}
#endif

void process_init(void) {
    spinlock_init(&g_process_lock);
    memset(g_processes, 0, sizeof(g_processes));
    memset(g_process_used, 0, sizeof(g_process_used));
    memset(g_current, 0, sizeof(g_current));
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
    spinlock_lock(&g_process_lock);
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_used[i]) continue;
        struct fd_table* fds = fd_table_create();
        if (!fds) { spinlock_unlock(&g_process_lock); return NULL; }

#ifndef TACH_HOST_TEST
        struct vmm_context* address_space = vmm_create_context();
        if (!address_space) {
            fd_table_destroy(fds);
            spinlock_unlock(&g_process_lock);
            return NULL;
        }
#endif
        struct process* proc = &g_processes[i];
        memset(proc, 0, sizeof(*proc));
        g_process_used[i] = true;
        proc->pid = g_next_pid++;
        struct process* current = process_get_current();
        proc->ppid = current ? current->pid : 0;
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
        spinlock_unlock(&g_process_lock);
        return proc;
    }
    spinlock_unlock(&g_process_lock);
    return NULL;
}

void process_destroy(struct process* proc) {
    if (!proc) return;
    spinlock_lock(&g_process_lock);
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (&g_processes[i] != proc || !g_process_used[i]) continue;
        scheduler_remove(proc);
        fd_table_destroy(proc->fds);
#ifndef TACH_HOST_TEST
        if (proc->stack && proc->stack_pages) {
            pmm_free_pages(proc->stack, proc->stack_pages);
        }
        if (proc->mm) {
            if (proc == process_get_current())
                vmm_switch_context(vmm_kernel_context());
            vmm_destroy_context(proc->mm);
        }
#endif
        for (size_t cpu = 0; cpu < MAX_CPUS; cpu++)
            if (g_current[cpu] == proc) g_current[cpu] = NULL;
        memset(proc, 0, sizeof(*proc));
        g_process_used[i] = false;
        spinlock_unlock(&g_process_lock);
        return;
    }
    spinlock_unlock(&g_process_lock);
}

struct process* process_get(pid_t pid) {
    spinlock_lock(&g_process_lock);
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_used[i] && g_processes[i].pid == pid) {
            struct process* result = &g_processes[i];
            spinlock_unlock(&g_process_lock);
            return result;
        }
    }
    spinlock_unlock(&g_process_lock);
    return NULL;
}

struct process* process_get_current(void) {
    return g_current[hal_smp_current_cpu()];
}
void process_set_current(struct process* proc) {
    uint32_t cpu = hal_smp_current_cpu();
    g_current[cpu] = proc;
    if (proc) {
        proc->cpu_id = cpu;
        proc->on_cpu = true;
    }
}

int process_start(struct process* proc, void* entry, void* arg) {
    if (!proc || !entry) return -EINVAL;
    proc->entry_point = entry;
    proc->entry_arg = arg;
#ifndef TACH_HOST_TEST
    int result = allocate_kernel_stack(proc);
    if (result < 0) return result;
#endif
    proc->state = PROCESS_STATE_RUNNING;
    scheduler_add(proc);
    return 0;
}

int process_start_user(struct process* process, void* entry,
                       uintptr_t stack_top) {
    if (!process || !entry || !process->mm) return -EINVAL;
    if (!arch_user_supported()) return -ENOTSUP;
    if (!stack_top) stack_top = PROCESS_USER_STACK_TOP;
    stack_top &= ~(uintptr_t)0xf;
#ifndef TACH_HOST_TEST
    int result = allocate_kernel_stack(process);
    if (result < 0) return result;
    uintptr_t stack_page = (stack_top - PAGE_SIZE) &
                           ~(uintptr_t)(PAGE_SIZE - 1);
    if (!vmm_resolve(process->mm, (void*)stack_page)) {
        result = vmm_map_allocated(process->mm, (void*)stack_page,
                                   VMM_USER | VMM_WRITABLE, NULL);
        if (result < 0) return result;
    }
#endif
    process->entry_point = entry;
    process->user_mode = true;
    process->user_stack_top = stack_top;
    process->user_stack_bottom = stack_top - PAGE_SIZE;
    memset(&process->user_context, 0, sizeof(process->user_context));
    process->user_context.pc = (uintptr_t)entry;
    process->user_context.sp = stack_top;
    process->user_context.flags = 0x202;
    process->state = PROCESS_STATE_RUNNING;
    scheduler_add(process);
    return 0;
}

struct process* process_fork(struct process* parent) {
    if (!parent) return NULL;
    struct process* child = process_create(parent->name);
    if (!child) return NULL;

    struct fd_table* descriptors = fd_table_clone(parent->fds);
    if (!descriptors) {
        process_destroy(child);
        return NULL;
    }
    fd_table_destroy(child->fds);
    child->fds = descriptors;
#ifndef TACH_HOST_TEST
    struct vmm_context* address_space = vmm_clone_context(parent->mm);
    if (!address_space) {
        process_destroy(child);
        return NULL;
    }
    vmm_destroy_context(child->mm);
    child->mm = address_space;
    child->mm->owner = child->pid;
    if (allocate_kernel_stack(child) < 0) {
        process_destroy(child);
        return NULL;
    }
#else
    child->mm = parent->mm;
#endif
    child->ppid = parent->pid;
    child->uid = parent->uid;
    child->gid = parent->gid;
    child->entry_point = parent->entry_point;
    child->entry_arg = parent->entry_arg;
    child->user_mode = parent->user_mode;
    child->user_stack_top = parent->user_stack_top;
    child->user_stack_bottom = parent->user_stack_bottom;
    child->brk_start = parent->brk_start;
    child->brk_end = parent->brk_end;
    child->user_context = parent->user_context;
    child->user_context.regs[0] = 0;
    memcpy(child->signal_actions, parent->signal_actions,
           sizeof(child->signal_actions));
    child->signal_blocked = parent->signal_blocked;
    child->state = PROCESS_STATE_RUNNING;
    scheduler_add(child);
    return child;
}

int process_exec_image_args(struct process* process, const void* image,
                            size_t size, const char* name,
                            const char* const argv[], size_t argc) {
#ifdef TACH_HOST_TEST
    (void)process; (void)image; (void)size; (void)name; (void)argv; (void)argc;
    return -ENOTSUP;
#else
    if (!arch_user_supported()) return -ENOTSUP;
    if (!process || !image || !size) return -EINVAL;
    struct vmm_context* new_context = vmm_create_context();
    if (!new_context) return -ENOMEM;
    struct process loader = *process;
    loader.mm = new_context;
    loader.entry_point = NULL;
    int result = elf_load(image, size, &loader);
    if (result < 0) {
        vmm_destroy_context(new_context);
        return result;
    }
    result = allocate_kernel_stack(process);
    if (result < 0) {
        vmm_destroy_context(new_context);
        return result;
    }
    uintptr_t stack_top = PROCESS_USER_STACK_TOP & ~(uintptr_t)0xf;
    uintptr_t stack_page = stack_top - PAGE_SIZE;
    phys_addr_t stack_physical;
    result = vmm_map_allocated(new_context, (void*)stack_page,
                               VMM_USER | VMM_WRITABLE, &stack_physical);
    if (result < 0) {
        vmm_destroy_context(new_context);
        return result;
    }
    if (argc > 16) {
        vmm_destroy_context(new_context);
        return -E2BIG;
    }
    uintptr_t argument_addresses[16];
    uintptr_t cursor = stack_top;
    for (size_t i = argc; i > 0; i--) {
        size_t length = strlen(argv[i - 1]) + 1;
        if (length > 256 || cursor - stack_page < length) {
            vmm_destroy_context(new_context);
            return -E2BIG;
        }
        cursor -= length;
        memcpy((void*)((uintptr_t)stack_physical + cursor - stack_page),
               argv[i - 1], length);
        argument_addresses[i - 1] = cursor;
    }
    cursor &= ~(uintptr_t)0xf;
    size_t words = argc + 3;
    uintptr_t initial_sp = (cursor - words * sizeof(uintptr_t)) &
                           ~(uintptr_t)0xf;
    if (initial_sp < stack_page) {
        vmm_destroy_context(new_context);
        return -E2BIG;
    }
    uintptr_t* initial = (uintptr_t*)((uintptr_t)stack_physical +
                         (initial_sp - stack_page));
    initial[0] = argc;
    for (size_t i = 0; i < argc; i++) initial[i + 1] = argument_addresses[i];
    initial[argc + 1] = 0;
    initial[argc + 2] = 0;

    struct vmm_context* old_context = process->mm;
    bool is_current = process == process_get_current();
    process->mm = new_context;
    process->mm->owner = process->pid;
    if (is_current) vmm_switch_context(new_context);
    if (old_context) vmm_destroy_context(old_context);
    process->entry_point = loader.entry_point;
    process->entry_arg = NULL;
    process->user_mode = true;
    process->exec_pending = true;
    process->user_stack_top = stack_top;
    process->user_stack_bottom = stack_page;
    process->brk_start = 0x20000000u;
    process->brk_end = process->brk_start;
    memset(&process->user_context, 0, sizeof(process->user_context));
    process->user_context.pc = (uintptr_t)loader.entry_point;
    process->user_context.sp = initial_sp;
    process->user_context.flags = 0x202;
    process->signal_pending = 0;
    process->state = PROCESS_STATE_RUNNING;
    scheduler_add(process);
    if (name && *name) {
        const char* base = name;
        for (const char* cursor = name; *cursor; cursor++)
            if (*cursor == '/') base = cursor + 1;
        strncpy(process->name, base, MAX_PROCESS_NAME - 1);
        process->name[MAX_PROCESS_NAME - 1] = '\0';
    }
    return 0;
#endif
}

int process_exec_image(struct process* process, const void* image, size_t size,
                       const char* name) {
    const char* argv[1];
    size_t argc = 0;
    if (name && *name) { argv[0] = name; argc = 1; }
    return process_exec_image_args(process, image, size, name, argv, argc);
}

pid_t process_waitpid(pid_t pid, int* status, int options) {
    struct process* parent = process_get_current();
    if (!parent) return (pid_t)-ESRCH;
    for (;;) {
        bool found = false;
        for (size_t i = 0; i < process_count(); i++) {
            struct process* child = process_at(i);
            if (!child || child->ppid != parent->pid) continue;
            if ((int32_t)pid != -1 && child->pid != pid) continue;
            found = true;
            if (child->state != PROCESS_STATE_ZOMBIE) continue;
            pid_t child_pid = child->pid;
            if (status) *status = child->exit_code;
            process_destroy(child);
            return child_pid;
        }
        if (!found) return (pid_t)-ECHILD;
        if (options & WNOHANG) return 0;
        scheduler_block_current((void*)(uintptr_t)pid);
    }
}

void process_terminate(struct process* process, int code) {
    if (!process || process->state == PROCESS_STATE_ZOMBIE) return;
    process->exit_code = code;
    process->state = PROCESS_STATE_ZOMBIE;
    scheduler_remove(process);
    struct process* parent = process_get(process->ppid);
    if (parent) {
        (void)signal_send(parent->pid, SIGCHLD);
        if (parent->state == PROCESS_STATE_BLOCKED) scheduler_wake(parent);
    }
}

void process_exit(int code) {
    struct process* current = process_get_current();
    if (!current) return;
    process_terminate(current, code);
#ifndef TACH_HOST_TEST
    scheduler_yield();
    for (;;) {}
#endif
}

void process_set_user_context(const struct user_context* context) {
    struct process* current = process_get_current();
    if (current && context) current->user_context = *context;
}

void process_get_user_context(struct user_context* context) {
    struct process* current = process_get_current();
    if (current && context) *context = current->user_context;
}

void process_wait(struct process* proc) {
    if (!proc) return;
    while (proc->state != PROCESS_STATE_ZOMBIE) scheduler_yield();
}

size_t process_count(void) {
    size_t count = 0;
    spinlock_lock(&g_process_lock);
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_process_used[i]) count++;
    }
    spinlock_unlock(&g_process_lock);
    return count;
}

struct process* process_at(size_t index) {
    size_t current = 0;
    spinlock_lock(&g_process_lock);
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (!g_process_used[i]) continue;
        if (current++ == index) {
            struct process* result = &g_processes[i];
            spinlock_unlock(&g_process_lock);
            return result;
        }
    }
    spinlock_unlock(&g_process_lock);
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
