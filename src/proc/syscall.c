#include <kernel/errno.h>
#include <kernel/string.h>
#include <proc/syscall.h>
#include <proc/process.h>
#include <proc/fd.h>
#include <fs/vfs.h>

static syscall_handler_t g_syscalls[SYSCALL_TABLE_SIZE];

static long sys_exit(uint64_t code, uint64_t unused2, uint64_t unused3,
                     uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused2; (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    process_exit((int)code);
    return 0;
}

static long sys_read(uint64_t fd, uint64_t buffer, uint64_t count,
                     uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused4; (void)unused5; (void)unused6;
    struct process* proc = process_get_current();
    if (!proc || !buffer) {
        return -EFAULT;
    }
    struct fd_entry* entry = fd_get(proc->fds, (int)fd);
    if (!entry || (entry->flags & O_ACCMODE) == O_WRONLY) {
        return -EBADF;
    }
    ssize_t result = vfs_read(entry->vnode, (void*)(uintptr_t)buffer,
                              (size_t)count, entry->offset);
    if (result > 0) {
        entry->offset += result;
    }
    return result;
}

static long sys_write(uint64_t fd, uint64_t buffer, uint64_t count,
                      uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused4; (void)unused5; (void)unused6;
    struct process* proc = process_get_current();
    if (!proc || !buffer) {
        return -EFAULT;
    }
    struct fd_entry* entry = fd_get(proc->fds, (int)fd);
    if (!entry || (entry->flags & O_ACCMODE) == O_RDONLY) {
        return -EBADF;
    }
    ssize_t result = vfs_write(entry->vnode, (const void*)(uintptr_t)buffer,
                               (size_t)count, entry->offset);
    if (result > 0) {
        entry->offset += result;
    }
    return result;
}

static long sys_open(uint64_t path, uint64_t flags, uint64_t unused3,
                     uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    struct process* proc = process_get_current();
    if (!proc || !path) {
        return -EFAULT;
    }
    struct vnode* vnode = vfs_open((const char*)(uintptr_t)path, (int)flags);
    if (!vnode) {
        return -ENOENT;
    }
    int fd = fd_alloc(proc->fds, vnode, (int)flags);
    if (fd < 0) {
        vfs_close(vnode);
    }
    return fd;
}

static long sys_close(uint64_t fd, uint64_t unused2, uint64_t unused3,
                      uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused2; (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    struct process* proc = process_get_current();
    if (!proc) {
        return -ESRCH;
    }
    struct fd_entry* entry = fd_get(proc->fds, (int)fd);
    if (!entry) {
        return -EBADF;
    }
    (void)vfs_close(entry->vnode);
    fd_put(proc->fds, (int)fd);
    return 0;
}

static long sys_getpid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                       uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused1; (void)unused2; (void)unused3;
    (void)unused4; (void)unused5; (void)unused6;
    struct process* proc = process_get_current();
    return proc ? (long)proc->pid : -ESRCH;
}

void syscall_init(void) {
    memset(g_syscalls, 0, sizeof(g_syscalls));
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_READ, sys_read);
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_OPEN, sys_open);
    syscall_register(SYS_CLOSE, sys_close);
    syscall_register(SYS_GETPID, sys_getpid);
}

long syscall_dispatch(int num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    if (num < 0 || num >= SYSCALL_TABLE_SIZE || !g_syscalls[num]) {
        return -ENOSYS;
    }
    return g_syscalls[num](arg1, arg2, arg3, arg4, arg5, arg6);
}

void syscall_register(int num, syscall_handler_t handler) {
    if (num >= 0 && num < SYSCALL_TABLE_SIZE) {
        g_syscalls[num] = handler;
    }
}

const char* syscall_name(int num) {
    switch (num) {
        case SYS_EXIT: return "exit";
        case SYS_READ: return "read";
        case SYS_WRITE: return "write";
        case SYS_OPEN: return "open";
        case SYS_CLOSE: return "close";
        case SYS_GETPID: return "getpid";
        default: return NULL;
    }
}
