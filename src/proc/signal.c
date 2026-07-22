#include <kernel/types.h>
#include <proc/signal.h>
int signal_register(int sig) {(void)sig; return 0;}
int signal_send(int pid, int sig) {(void)pid;(void)sig; return 0;}
