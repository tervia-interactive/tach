/*
 * proc/process.h - PCB (Process Control Block), process states
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_PROCESS_H
#define _PROC_PROCESS_H

#include <kernel/types.h>
#include <kernel/signal.h>
#include <proc/fd.h>

struct vmm_context;

#define PROCESS_STATE_UNUSED    0
#define PROCESS_STATE_RUNNING   1
#define PROCESS_STATE_SLEEPING  2
#define PROCESS_STATE_STOPPED   3
#define PROCESS_STATE_ZOMBIE    4
#define PROCESS_STATE_BLOCKED   5

#define MAX_PROCESS_NAME 64
#define MAX_OPEN_FILES 256
#define MAX_PROCESSES 32
#define PROCESS_USER_STACK_TOP ((uintptr_t)0x000000007ff00000ULL)
#define PROCESS_USER_STACK_MAX (8u * 1024u * 1024u)

struct user_context {
    uintptr_t pc;
    uintptr_t sp;
    uintptr_t flags;
    uintptr_t regs[16];
};

struct process {
    pid_t pid;
    pid_t ppid;
    char name[MAX_PROCESS_NAME];
    int state;
    void* stack;
    void* entry_point;
    struct vmm_context* mm;
    struct fd_table* fds;
    uid_t uid;
    gid_t gid;
    int exit_code;
    size_t refcount;
    void* entry_arg;
    uint64_t runtime_ticks;
    uintptr_t saved_stack;
    size_t stack_pages;
    void* block_reason;
    bool user_mode;
    bool exec_pending;
    uintptr_t user_stack_top;
    uintptr_t user_stack_bottom;
    uintptr_t brk_start;
    uintptr_t brk_end;
    struct user_context user_context;
    sigaction_t signal_actions[32];
    uint32_t signal_pending;
    uint32_t signal_blocked;
    uintptr_t signal_fault_address;
};

void process_init(void);
struct process* process_create(const char* name);
void process_destroy(struct process* proc);
struct process* process_get(pid_t pid);
struct process* process_get_current(void);
void process_set_current(struct process* proc);
int process_start(struct process* proc, void* entry, void* arg);
int process_start_user(struct process* proc, void* entry, uintptr_t stack_top);
struct process* process_fork(struct process* parent);
int process_exec_image(struct process* proc, const void* image, size_t size,
                       const char* name);
pid_t process_waitpid(pid_t pid, int* status, int options);
void process_exit(int code);
void process_terminate(struct process* proc, int code);
void process_wait(struct process* proc);
size_t process_count(void);
struct process* process_at(size_t index);
const char* process_state_name(int state);
void process_set_user_context(const struct user_context* context);
void process_get_user_context(struct user_context* context);

void arch_user_init(void);
bool arch_user_supported(void);
void arch_set_kernel_stack(uintptr_t stack_top);
void arch_enter_user(struct user_context* context) __attribute__((noreturn));

#endif /* _PROC_PROCESS_H */
