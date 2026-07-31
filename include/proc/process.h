/*
 * proc/process.h - PCB (Process Control Block), process states
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_PROCESS_H
#define _PROC_PROCESS_H

#include <kernel/types.h>
#include <proc/fd.h>

#define PROCESS_STATE_UNUSED    0
#define PROCESS_STATE_RUNNING   1
#define PROCESS_STATE_SLEEPING  2
#define PROCESS_STATE_STOPPED   3
#define PROCESS_STATE_ZOMBIE    4

#define MAX_PROCESS_NAME 64
#define MAX_OPEN_FILES 256
#define MAX_PROCESSES 32

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
};

void process_init(void);
struct process* process_create(const char* name);
void process_destroy(struct process* proc);
struct process* process_get(pid_t pid);
struct process* process_get_current(void);
void process_set_current(struct process* proc);
int process_start(struct process* proc, void* entry, void* arg);
void process_exit(int code);
void process_wait(struct process* proc);
size_t process_count(void);
struct process* process_at(size_t index);
const char* process_state_name(int state);

#endif /* _PROC_PROCESS_H */
