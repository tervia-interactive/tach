/* tach - Process signal state, default actions, and user handler delivery. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/signal.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>

static bool valid_signal(int signal) { return signal > 0 && signal < 32; }
static uint32_t signal_bit(int signal) { return 1u << (unsigned)signal; }

void sigemptyset(sigset_t* set) { if (set) memset(set, 0, sizeof(*set)); }
void sigfillset(sigset_t* set) { if (set) memset(set, 0xff, sizeof(*set)); }
void sigaddset(sigset_t* set, int signal) {
    if (set && valid_signal(signal))
        set->bits[(unsigned)signal / 32u] |= 1u << ((unsigned)signal % 32u);
}
void sigdelset(sigset_t* set, int signal) {
    if (set && valid_signal(signal))
        set->bits[(unsigned)signal / 32u] &= ~(1u << ((unsigned)signal % 32u));
}
int sigismember(const sigset_t* set, int signal) {
    if (!set || !valid_signal(signal)) return 0;
    return (set->bits[(unsigned)signal / 32u] &
            (1u << ((unsigned)signal % 32u))) != 0;
}

int signal_register(int signal) {
    return valid_signal(signal) ? 0 : -EINVAL;
}

int signal_set_action(struct process* process, int signal,
                      const sigaction_t* action, sigaction_t* old_action) {
    if (!process || !valid_signal(signal)) return -EINVAL;
    if (old_action) *old_action = process->signal_actions[signal];
    if (!action) return 0;
    if (signal == SIGKILL || signal == SIGSTOP) return -EINVAL;
    process->signal_actions[signal] = *action;
    return 0;
}

int signal_send(pid_t pid, int signal) {
    if (!valid_signal(signal)) return -EINVAL;
    struct process* process = process_get(pid);
    if (!process) return -ESRCH;
    if (signal == SIGKILL) {
        process_terminate(process, 128 + signal);
        return 0;
    }
    if (signal == SIGCONT && process->state == PROCESS_STATE_STOPPED) {
        process->state = PROCESS_STATE_RUNNING;
        scheduler_add(process);
    }
    process->signal_pending |= signal_bit(signal);
    if (process->state == PROCESS_STATE_BLOCKED) scheduler_wake(process);
    return 0;
}

static bool ignored_by_default(int signal) {
    return signal == SIGCHLD || signal == SIGWINCH || signal == SIGURG ||
           signal == SIGCONT;
}

int signal_deliver_pending(struct process* process,
                           struct user_context* context) {
    if (!process || !context) return -EINVAL;
    uint32_t available = process->signal_pending & ~process->signal_blocked;
    if (!available) return 0;
    int signal = 1;
    while (signal < 32 && !(available & signal_bit(signal))) signal++;
    if (signal >= 32) return 0;
    process->signal_pending &= ~signal_bit(signal);

    sigaction_t* action = &process->signal_actions[signal];
    if (action->sa_handler == SIG_IGN && signal != SIGKILL && signal != SIGSTOP)
        return signal;
    if (signal == SIGSTOP || signal == SIGTSTP || signal == SIGTTIN ||
        signal == SIGTTOU) {
        process->state = PROCESS_STATE_STOPPED;
        scheduler_remove(process);
        return signal;
    }
    if (action->sa_handler == SIG_DFL) {
        if (ignored_by_default(signal)) return signal;
        process_terminate(process, 128 + signal);
        return signal;
    }

#if defined(__x86_64__) || defined(TACH_HOST_TEST)
    uintptr_t new_stack = context->sp - sizeof(uintptr_t);
#ifdef TACH_HOST_TEST
    phys_addr_t physical = new_stack;
#else
    phys_addr_t physical = vmm_resolve(process->mm, (void*)new_stack);
#endif
    if (!physical) {
        process_terminate(process, 128 + SIGSEGV);
        return -EFAULT;
    }
    *(uintptr_t*)(uintptr_t)physical = context->pc;
    context->sp = new_stack;
    context->pc = (uintptr_t)action->sa_handler;
    context->regs[1] = (uintptr_t)signal;
#elif defined(__i386__)
    /* A tiny user-stack restorer removes the cdecl signal argument before
     * returning to the interrupted PC: add $4, %esp; ret. */
    uint8_t frame[4 * sizeof(uintptr_t)];
    memset(frame, 0, sizeof(frame));
    frame[0] = 0x83; frame[1] = 0xc4; frame[2] = 0x04; frame[3] = 0xc3;
    uintptr_t new_stack = context->sp - sizeof(frame);
    uintptr_t signal_value = (uintptr_t)signal;
    memcpy(frame + sizeof(uintptr_t), &new_stack, sizeof(new_stack));
    memcpy(frame + 2 * sizeof(uintptr_t), &signal_value,
           sizeof(signal_value));
    memcpy(frame + 3 * sizeof(uintptr_t), &context->pc,
           sizeof(context->pc));
    if (vmm_copy_to_user(process->mm, (void*)new_stack,
                         frame, sizeof(frame)) < 0) {
        process_terminate(process, 128 + SIGSEGV);
        return -EFAULT;
    }
    context->sp = new_stack + sizeof(uintptr_t);
    context->pc = (uintptr_t)action->sa_handler;
#else
    process->signal_pending |= signal_bit(signal);
    return -ENOTSUP;
#endif
    return signal;
}
