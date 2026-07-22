#include <kernel/types.h>
#include <proc/wait.h>
pid_t wait(int *status) {(void)status; return 0;}
pid_t waitpid(pid_t pid, int *status, int options) {(void)pid;(void)status;(void)options; return 0;}
