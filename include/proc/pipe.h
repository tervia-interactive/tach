/*
 * proc/pipe.h - Anonymous pipes (classic Unix IPC)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_PIPE_H
#define _PROC_PIPE_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <fs/vfs.h>

#define PIPE_BUFFER_SIZE 4096
#define PIPE_MAX_PIPES   16

struct pipe {
    char buffer[PIPE_BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t data_size;
    int readers;            /* open read-end descriptors */
    int writers;            /* open write-end descriptors */
    spinlock_t lock;
    bool in_use;
    struct vnode read_end;  /* vnode behind the read descriptor */
    struct vnode write_end; /* vnode behind the write descriptor */
};

/*
 * Create a pipe in the current process' descriptor table.
 * fds[0] receives the read end and fds[1] the write end. Returns 0 or -errno.
 */
int pipe_create(int fds[2]);

/*
 * Blocking read. Returns the bytes available (at least 1), 0 at end of file
 * (no data and no writers left) or -errno (-EINTR if a signal is pending).
 */
ssize_t pipe_read(struct pipe* p, void* buf, size_t count);

/*
 * Blocking write. Writes everything unless the readers go away or a signal
 * interrupts it. With no readers it raises SIGPIPE and returns -EPIPE.
 */
ssize_t pipe_write(struct pipe* p, const void* buf, size_t count);

/* Drop one read/write descriptor reference (the last one frees the pipe). */
void pipe_close_read(struct pipe* p);
void pipe_close_write(struct pipe* p);

#endif /* _PROC_PIPE_H */
