/*
 * proc/pipe.h - Anonymous pipes (classic Unix IPC)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_PIPE_H
#define _PROC_PIPE_H

#include <kernel/types.h>

#define PIPE_BUFFER_SIZE 4096

struct pipe {
    char buffer[PIPE_BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t data_size;
    int readers;
    int writers;
};

int pipe_create(int fds[2]);
ssize_t pipe_read(struct pipe* p, void* buf, size_t count);
ssize_t pipe_write(struct pipe* p, const void* buf, size_t count);
void pipe_close_read(struct pipe* p);
void pipe_close_write(struct pipe* p);

#endif /* _PROC_PIPE_H */
