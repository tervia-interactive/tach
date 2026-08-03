#include <kernel/errno.h>
#include <kernel/string.h>
#include <proc/syscall.h>
#include <proc/process.h>
#include <proc/fd.h>
#include <fs/vfs.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel/signal.h>

static syscall_handler_t g_syscalls[SYSCALL_TABLE_SIZE];

static int copy_from_caller(struct process* process, void* destination,
                            const void* source, size_t size) {
    if (!process || !destination || (!source && size)) return -EFAULT;
#ifdef TACH_HOST_TEST
    memcpy(destination, source, size);
    return 0;
#else
    if (!process->user_mode) {
        memcpy(destination, source, size);
        return 0;
    }
    return vmm_copy_from_user(destination, process->mm, source, size);
#endif
}

static int copy_to_caller(struct process* process, void* destination,
                          const void* source, size_t size) {
    if (!process || (!destination && size) || !source) return -EFAULT;
#ifdef TACH_HOST_TEST
    memcpy(destination, source, size);
    return 0;
#else
    if (!process->user_mode) {
        memcpy(destination, source, size);
        return 0;
    }
    return vmm_copy_to_user(process->mm, destination, source, size);
#endif
}

static int copy_path_from_caller(struct process* process, char* destination,
                                 size_t capacity, const char* source) {
    if (!process || !destination || !capacity || !source) return -EFAULT;
#ifdef TACH_HOST_TEST
    if (strlen(source) >= capacity) return -ENAMETOOLONG;
    strcpy(destination, source);
    return 0;
#else
    if (process->user_mode)
        return vmm_copy_string_from_user(destination, capacity,
                                         process->mm, source);
    if (strlen(source) >= capacity) return -ENAMETOOLONG;
    strcpy(destination, source);
    return 0;
#endif
}

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
    uint8_t temporary[256];
    size_t total = 0;
    while (total < (size_t)count) {
        size_t chunk = (size_t)count - total;
        if (chunk > sizeof(temporary)) chunk = sizeof(temporary);
        ssize_t result = vfs_read(entry->vnode, temporary, chunk,
                                  entry->offset);
        if (result <= 0) return total ? (long)total : result;
        if (copy_to_caller(proc, (uint8_t*)(uintptr_t)buffer + total,
                           temporary, (size_t)result) < 0) return -EFAULT;
        entry->offset += result;
        total += (size_t)result;
        if ((size_t)result < chunk) break;
    }
    return (long)total;
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
    uint8_t temporary[256];
    size_t total = 0;
    while (total < (size_t)count) {
        size_t chunk = (size_t)count - total;
        if (chunk > sizeof(temporary)) chunk = sizeof(temporary);
        if (copy_from_caller(proc, temporary,
                (const uint8_t*)(uintptr_t)buffer + total, chunk) < 0)
            return -EFAULT;
        ssize_t result = vfs_write(entry->vnode, temporary, chunk,
                                   entry->offset);
        if (result <= 0) return total ? (long)total : result;
        entry->offset += result;
        total += (size_t)result;
        if ((size_t)result < chunk) break;
    }
    return (long)total;
}

static long sys_open(uint64_t path, uint64_t flags, uint64_t unused3,
                     uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    struct process* proc = process_get_current();
    if (!proc || !path) {
        return -EFAULT;
    }
    char local_path[256];
    int copied = copy_path_from_caller(proc, local_path, sizeof(local_path),
                                       (const char*)(uintptr_t)path);
    if (copied < 0) return copied;
    struct vnode* vnode = vfs_open(local_path, (int)flags);
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

static long sys_fork(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                     uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused1; (void)unused2; (void)unused3;
    (void)unused4; (void)unused5; (void)unused6;
    struct process* child = process_fork(process_get_current());
    return child ? (long)child->pid : -ENOMEM;
}

static long sys_execve(uint64_t path, uint64_t argv, uint64_t envp,
                       uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)envp; (void)unused4; (void)unused5; (void)unused6;
    if (!path) return -EFAULT;
    struct process* process = process_get_current();
    char local_path[256];
    int copied = copy_path_from_caller(process, local_path,
        sizeof(local_path), (const char*)(uintptr_t)path);
    if (copied < 0) return copied;
    const void* image;
    size_t size;
    int result = vfs_read_file(local_path, &image, &size);
    if (result < 0) return result;
    char argument_storage[16][128];
    const char* arguments[16];
    size_t argc = 0;
    if (argv) {
        for (; argc < 16; argc++) {
            uintptr_t user_argument = 0;
            result = copy_from_caller(process, &user_argument,
                (const void*)((uintptr_t)argv + argc * sizeof(uintptr_t)),
                sizeof(user_argument));
            if (result < 0) return result;
            if (!user_argument) break;
            result = copy_path_from_caller(process, argument_storage[argc],
                sizeof(argument_storage[argc]),
                (const char*)user_argument);
            if (result < 0) return result;
            arguments[argc] = argument_storage[argc];
        }
        if (argc == 16) return -E2BIG;
    }
    return process_exec_image_args(process, image, size, local_path,
                                   arguments, argc);
}

static long sys_waitpid(uint64_t pid, uint64_t status, uint64_t options,
                        uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused4; (void)unused5; (void)unused6;
    int local_status = 0;
    pid_t result = process_waitpid((pid_t)pid,
                                   status ? &local_status : NULL,
                                   (int)options);
    if ((int32_t)result > 0 && status &&
        copy_to_caller(process_get_current(), (void*)(uintptr_t)status,
                       &local_status, sizeof(local_status)) < 0)
        return -EFAULT;
    return (long)(int32_t)result;
}

static long sys_kill(uint64_t pid, uint64_t signal, uint64_t unused3,
                     uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    return signal_send((pid_t)pid, (int)signal);
}

static long sys_sigaction(uint64_t signal, uint64_t action, uint64_t old,
                          uint64_t unused4, uint64_t unused5,
                          uint64_t unused6) {
    (void)unused4; (void)unused5; (void)unused6;
    struct process* process = process_get_current();
    sigaction_t local_action;
    sigaction_t old_action;
    const sigaction_t* action_pointer = NULL;
    if (action) {
        int result = copy_from_caller(process, &local_action,
            (const void*)(uintptr_t)action, sizeof(local_action));
        if (result < 0) return result;
        action_pointer = &local_action;
    }
    int result = signal_set_action(process, (int)signal, action_pointer,
                                   old ? &old_action : NULL);
    if (result < 0) return result;
    if (old) return copy_to_caller(process, (void*)(uintptr_t)old,
                                   &old_action, sizeof(old_action));
    return 0;
}

static long sys_brk(uint64_t address, uint64_t unused2, uint64_t unused3,
                    uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused2; (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    struct process* process = process_get_current();
    if (!process) return -ESRCH;
    if (!address) return (long)process->brk_end;
    if ((uintptr_t)address < process->brk_start ||
        (uintptr_t)address >= process->user_stack_bottom - PAGE_SIZE)
        return -ENOMEM;
    process->brk_end = (uintptr_t)address;
    return (long)process->brk_end;
}

static long sys_mmap(uint64_t address, uint64_t length, uint64_t protection,
                     uint64_t flags, uint64_t fd, uint64_t offset) {
#ifdef TACH_HOST_TEST
    (void)address; (void)length; (void)protection;
    (void)flags; (void)fd; (void)offset;
    return -ENOTSUP;
#else
    (void)flags; (void)fd; (void)offset;
    struct process* process = process_get_current();
    if (!process || !process->mm || !length) return -EINVAL;
    if ((size_t)length > (size_t)-1 - (PAGE_SIZE - 1)) return -EOVERFLOW;
    size_t pages = ((size_t)length + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t vmm_flags = VMM_USER;
    if (protection & 2u) vmm_flags |= VMM_WRITABLE;
    if (protection & 4u) vmm_flags |= VMM_EXECUTABLE;
    uintptr_t base = (uintptr_t)address;
    size_t mapped = 0;
    if (!base) {
        void* first = vmm_alloc_page(process->mm, vmm_flags);
        if (!first) return -ENOMEM;
        base = (uintptr_t)first;
        mapped = 1;
        for (size_t i = 1; i < pages; i++) {
            if (vmm_map_allocated(process->mm,
                    (void*)(base + i * PAGE_SIZE), vmm_flags, NULL) < 0) {
                while (mapped)
                    (void)vmm_unmap(process->mm,
                                    (void*)(base + --mapped * PAGE_SIZE));
                return -ENOMEM;
            }
            mapped++;
        }
    } else {
        base &= ~(uintptr_t)(PAGE_SIZE - 1);
        if (pages > ((uintptr_t)-1 - base) / PAGE_SIZE) return -EOVERFLOW;
        for (size_t i = 0; i < pages; i++)
            if (vmm_resolve(process->mm,
                            (void*)(base + i * PAGE_SIZE))) return -EEXIST;
        for (size_t i = 0; i < pages; i++) {
            if (vmm_map_allocated(process->mm,
                    (void*)(base + i * PAGE_SIZE), vmm_flags, NULL) < 0) {
                while (mapped)
                    (void)vmm_unmap(process->mm,
                                    (void*)(base + --mapped * PAGE_SIZE));
                return -ENOMEM;
            }
            mapped++;
        }
    }
    return (long)base;
#endif
}

static long sys_munmap(uint64_t address, uint64_t length, uint64_t unused3,
                       uint64_t unused4, uint64_t unused5, uint64_t unused6) {
#ifdef TACH_HOST_TEST
    (void)address; (void)length; (void)unused3;
    (void)unused4; (void)unused5; (void)unused6;
    return -ENOTSUP;
#else
    (void)unused3; (void)unused4; (void)unused5; (void)unused6;
    struct process* process = process_get_current();
    if (!process || !process->mm || !address || !length) return -EINVAL;
    size_t pages = ((size_t)length + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t base = (uintptr_t)address & ~(uintptr_t)(PAGE_SIZE - 1);
    for (size_t i = 0; i < pages; i++)
        (void)vmm_unmap(process->mm, (void*)(base + i * PAGE_SIZE));
    return 0;
#endif
}

void syscall_init(void) {
    memset(g_syscalls, 0, sizeof(g_syscalls));
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_READ, sys_read);
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_OPEN, sys_open);
    syscall_register(SYS_CLOSE, sys_close);
    syscall_register(SYS_GETPID, sys_getpid);
    syscall_register(SYS_FORK, sys_fork);
    syscall_register(SYS_EXECVE, sys_execve);
    syscall_register(SYS_WAITPID, sys_waitpid);
    syscall_register(SYS_KILL, sys_kill);
    syscall_register(SYS_SIGACTION, sys_sigaction);
    syscall_register(SYS_BRK, sys_brk);
    syscall_register(SYS_MMAP, sys_mmap);
    syscall_register(SYS_MUNMAP, sys_munmap);
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
        case SYS_FORK: return "fork";
        case SYS_EXECVE: return "execve";
        case SYS_WAITPID: return "waitpid";
        case SYS_KILL: return "kill";
        case SYS_SIGACTION: return "sigaction";
        case SYS_BRK: return "brk";
        case SYS_MMAP: return "mmap";
        case SYS_MUNMAP: return "munmap";
        default: return NULL;
    }
}
