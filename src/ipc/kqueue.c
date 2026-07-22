#include <kernel/kqueue.h>
int kqueue_create(void) { return 0; }
int kqueue_add(int kq, int fd, int filter) {(void)kq;(void)fd;(void)filter; return 0;}
int kqueue_wait(int kq, void *events, int nevents) {(void)kq;(void)events;(void)nevents; return 0;}
