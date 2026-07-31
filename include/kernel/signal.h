/* tach - Signal Header */
/* POSIX-style signals (SIGKILL, SIGTERM, SIGCHLD, ...) */

#ifndef _KERNEL_SIGNAL_H
#define _KERNEL_SIGNAL_H

#include "kernel/types.h"

/* Standard signals */
#define SIGHUP       1   /* Hangup */
#define SIGINT       2   /* Interrupt */
#define SIGQUIT      3   /* Quit */
#define SIGILL       4   /* Illegal instruction */
#define SIGTRAP      5   /* Trace/breakpoint trap */
#define SIGABRT      6   /* Abort */
#define SIGBUS       7   /* Bus error */
#define SIGFPE       8   /* Floating point exception */
#define SIGKILL      9   /* Kill (cannot be caught) */
#define SIGUSR1     10   /* User-defined signal 1 */
#define SIGSEGV     11   /* Segmentation violation */
#define SIGUSR2     12   /* User-defined signal 2 */
#define SIGPIPE     13   /* Broken pipe */
#define SIGALRM     14   /* Alarm clock */
#define SIGTERM     15   /* Termination */
#define SIGCHLD     17   /* Child status change */
#define SIGCONT     18   /* Continue if stopped */
#define SIGSTOP     19   /* Stop (cannot be caught) */
#define SIGTSTP     20   /* Terminal stop */
#define SIGTTIN     21   /* Background read from tty */
#define SIGTTOU     22   /* Background write to tty */
#define SIGURG      23   /* Urgent socket condition */
#define SIGXCPU     24   /* CPU time limit exceeded */
#define SIGXFSZ     25   /* File size limit exceeded */
#define SIGVTALRM   26   /* Virtual timer expired */
#define SIGPROF     27   /* Profiling timer expired */
#define SIGWINCH    28   /* Window size change */
#define SIGINFO     29   /* Information request */

/* Signal actions */
#define SIG_DFL     ((void*)0)
#define SIG_IGN     ((void*)1)
#define SIG_ERR     ((void*)-1)

/* Signal set */
typedef struct {
    uint32_t bits[8];
} sigset_t;

/* Signal action structure */
typedef struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
} sigaction_t;

/* Signal info */
typedef struct siginfo {
    int si_signo;
    int si_code;
    int si_errno;
    pid_t si_pid;
    uid_t si_uid;
    void* si_addr;
} siginfo_t;

struct process;
struct user_context;

/* Initialize signal set */
void sigemptyset(sigset_t* set);
void sigfillset(sigset_t* set);
void sigaddset(sigset_t* set, int signo);
void sigdelset(sigset_t* set, int signo);
int sigismember(const sigset_t* set, int signo);
int signal_register(int sig);
int signal_send(pid_t pid, int sig);
int signal_set_action(struct process* proc, int sig,
                      const sigaction_t* action, sigaction_t* old_action);
int signal_deliver_pending(struct process* proc,
                           struct user_context* context);

#endif /* _KERNEL_SIGNAL_H */
