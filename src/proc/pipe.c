/*
 * proc/pipe.c - Anonymous pipes
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * A pipe is a 4 KiB ring buffer with two vnodes, one per end. The fd layer
 * calls the `dup`/`release` vnode hooks whenever a descriptor referencing an
 * end is duplicated (dup, dup2, fork) or dropped (close, process exit), which
 * keeps `readers`/`writers` equal to the number of open descriptors.
 *
 * Waiting is cooperative: a blocked reader/writer yields to the scheduler and
 * re-checks, so no wait queue has to be kept in sync with process teardown.
 */
#include <kernel/errno.h>
#include <kernel/signal.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <fs/vfs.h>
#include <proc/fd.h>
#include <proc/pipe.h>
#include <proc/process.h>
#include <proc/scheduler.h>

static struct pipe g_pipes[PIPE_MAX_PIPES];
static spinlock_t g_pipe_lock; /* zero-initialised == unlocked */

static struct pipe* pipe_from_vnode(struct vnode* vnode) {
    return vnode ? (struct pipe*)vnode->data : NULL;
}

/* True when the current process has a deliverable signal waiting. */
static bool pipe_interrupted(void) {
    struct process* current = process_get_current();
    return current && (current->signal_pending & ~current->signal_blocked);
}

/* Adjust the open-descriptor count of one end; free the pipe at zero/zero. */
static void pipe_adjust(struct pipe* p, bool reader, int delta) {
    if (!p) return;
    spinlock_lock(&p->lock);
    if (reader) {
        p->readers += delta;
        if (p->readers < 0) p->readers = 0;
    } else {
        p->writers += delta;
        if (p->writers < 0) p->writers = 0;
    }
    bool dead = p->readers == 0 && p->writers == 0;
    spinlock_unlock(&p->lock);
    if (dead) {
        spinlock_lock(&g_pipe_lock);
        p->in_use = false;
        spinlock_unlock(&g_pipe_lock);
    }
}

void pipe_close_read(struct pipe* p) { pipe_adjust(p, true, -1); }
void pipe_close_write(struct pipe* p) { pipe_adjust(p, false, -1); }

ssize_t pipe_read(struct pipe* p, void* buf, size_t count) {
    if (!p || (!buf && count)) return -EFAULT;
    if (!count) return 0;
    char* out = (char*)buf;
    for (;;) {
        spinlock_lock(&p->lock);
        if (p->data_size) {
            size_t n = count < p->data_size ? count : p->data_size;
            for (size_t i = 0; i < n; i++) {
                out[i] = p->buffer[p->read_pos];
                p->read_pos = (p->read_pos + 1) % PIPE_BUFFER_SIZE;
            }
            p->data_size -= n;
            spinlock_unlock(&p->lock);
            return (ssize_t)n;
        }
        if (p->writers <= 0) {
            spinlock_unlock(&p->lock);
            return 0; /* EOF */
        }
        spinlock_unlock(&p->lock);
        if (pipe_interrupted()) return -EINTR;
        scheduler_yield();
    }
}

ssize_t pipe_write(struct pipe* p, const void* buf, size_t count) {
    if (!p || (!buf && count)) return -EFAULT;
    const char* in = (const char*)buf;
    size_t total = 0;
    while (total < count) {
        spinlock_lock(&p->lock);
        if (p->readers <= 0) {
            spinlock_unlock(&p->lock);
            if (total) return (ssize_t)total;
            struct process* current = process_get_current();
            if (current) (void)signal_send(current->pid, SIGPIPE);
            return -EPIPE;
        }
        size_t space = PIPE_BUFFER_SIZE - p->data_size;
        if (space) {
            size_t remaining = count - total;
            size_t n = remaining < space ? remaining : space;
            for (size_t i = 0; i < n; i++) {
                p->buffer[p->write_pos] = in[total + i];
                p->write_pos = (p->write_pos + 1) % PIPE_BUFFER_SIZE;
            }
            p->data_size += n;
            total += n;
            spinlock_unlock(&p->lock);
            continue;
        }
        spinlock_unlock(&p->lock);
        if (pipe_interrupted()) return total ? (ssize_t)total : -EINTR;
        scheduler_yield();
    }
    return (ssize_t)total;
}

/* ---- vnode glue ---------------------------------------------------------- */

static ssize_t pipe_vnode_read(struct vnode* vnode, void* buf, size_t count,
                               int64_t offset) {
    (void)offset;
    return pipe_read(pipe_from_vnode(vnode), buf, count);
}

static ssize_t pipe_vnode_write(struct vnode* vnode, const void* buf,
                                size_t count, int64_t offset) {
    (void)offset;
    return pipe_write(pipe_from_vnode(vnode), buf, count);
}

static void pipe_read_dup(struct vnode* vnode) {
    pipe_adjust(pipe_from_vnode(vnode), true, 1);
}

static void pipe_read_release(struct vnode* vnode) {
    pipe_close_read(pipe_from_vnode(vnode));
}

static void pipe_write_dup(struct vnode* vnode) {
    pipe_adjust(pipe_from_vnode(vnode), false, 1);
}

static void pipe_write_release(struct vnode* vnode) {
    pipe_close_write(pipe_from_vnode(vnode));
}

static struct vnode_ops g_pipe_read_ops = {
    .open = NULL, .close = NULL,
    .read = pipe_vnode_read, .write = NULL, .readdir = NULL,
    .dup = pipe_read_dup, .release = pipe_read_release
};

static struct vnode_ops g_pipe_write_ops = {
    .open = NULL, .close = NULL,
    .read = NULL, .write = pipe_vnode_write, .readdir = NULL,
    .dup = pipe_write_dup, .release = pipe_write_release
};

static void pipe_init_end(struct vnode* vnode, struct pipe* p,
                          struct vnode_ops* ops, const char* name) {
    memset(vnode, 0, sizeof(*vnode));
    vnode->type = VNODE_PIPE;
    vnode->name = (char*)name;
    vnode->data = p;
    vnode->ops = ops;
}

static struct pipe* pipe_alloc(void) {
    spinlock_lock(&g_pipe_lock);
    for (size_t i = 0; i < PIPE_MAX_PIPES; i++) {
        struct pipe* p = &g_pipes[i];
        if (p->in_use) continue;
        memset(p, 0, sizeof(*p));
        spinlock_init(&p->lock);
        p->in_use = true;
        p->readers = 1;  /* owned by the descriptors pipe_create() installs */
        p->writers = 1;
        pipe_init_end(&p->read_end, p, &g_pipe_read_ops, "pipe:r");
        pipe_init_end(&p->write_end, p, &g_pipe_write_ops, "pipe:w");
        spinlock_unlock(&g_pipe_lock);
        return p;
    }
    spinlock_unlock(&g_pipe_lock);
    return NULL;
}

int pipe_create(int fds[2]) {
    struct process* current = process_get_current();
    if (!fds) return -EFAULT;
    if (!current || !current->fds) return -ESRCH;
    struct pipe* p = pipe_alloc();
    if (!p) return -ENFILE;

    int read_fd = fd_alloc(current->fds, &p->read_end, O_RDONLY);
    if (read_fd < 0) {
        pipe_close_read(p);
        pipe_close_write(p);
        return read_fd;
    }
    int write_fd = fd_alloc(current->fds, &p->write_end, O_WRONLY);
    if (write_fd < 0) {
        fd_put(current->fds, read_fd); /* release hook drops the read end */
        pipe_close_write(p);
        return write_fd;
    }
    fds[0] = read_fd;
    fds[1] = write_fd;
    return 0;
}
