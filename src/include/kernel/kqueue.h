/* tach - Kqueue Header */
/* Unified event notification (BSD-kqueue style), used by tachd */

#ifndef _KERNEL_KQUEUE_H
#define _KERNEL_KQUEUE_H

#include "kernel/types.h"

/* Event filter types */
#define EVFILT_READ     1
#define EVFILT_WRITE    2
#define EVFILT_TIMER    3
#define EVFILT_SIGNAL   4
#define EVFILT_PROC     5

/* Event flags */
#define EV_ADD      0x0001
#define EV_DELETE   0x0002
#define EV_ENABLE   0x0004
#define EV_DISABLE  0x0008
#define EV_ONESHOT  0x0010
#define EV_CLEAR    0x0020

/* Event structure */
typedef struct kevent {
    uintptr_t ident;
    int16_t filter;
    uint16_t flags;
    uint32_t fflags;
    intptr_t data;
    void* udata;
} kevent_t;

/* Create kqueue */
int kqueue_create(void);

/* Register/deregister events */
int kevent(int kq, const kevent_t* changelist, int nchanges,
           kevent_t* eventlist, int nevents, void* timeout);

/* Close kqueue */
void kqueue_close(int kq);

#endif /* _KERNEL_KQUEUE_H */
