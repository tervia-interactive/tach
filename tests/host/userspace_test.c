/*
 * Host-side integration test for the embedded userspace foundation.
 * This deliberately replaces the hardware console with deterministic
 * memory-backed input/output.
 */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <fs/vfs.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>
#include <ipc/sem.h>
#include <hal/time.h>
#include <kernel/signal.h>
#include <term/tty.h>
#include <term/vterm.h>
#include <userland/runtime.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static const char* g_input;
static size_t g_input_pos;
static char g_output[16384];
static size_t g_output_pos;

void kernel_panic_with_code(const char* reason, int code) {
    (void)reason;
    (void)code;
    __builtin_trap();
}

size_t pmm_get_total_pages(void) { return 1024; }
size_t pmm_get_free_pages(void) { return 768; }

void hal_console_early_init(void) {}

void hal_console_lock(void) {}
void hal_console_unlock(void) {}

void hal_console_putchar(char c) {
    if (g_output_pos + 1 < sizeof(g_output)) {
        g_output[g_output_pos++] = c;
        g_output[g_output_pos] = '\0';
    }
}

void hal_console_write(const char* str, size_t len) {
    while (len--) {
        hal_console_putchar(*str++);
    }
}

void hal_console_clear(void) {
    hal_console_write("[clear]\n", 8);
}

int hal_console_input_available(void) {
    return g_input && g_input[g_input_pos] != '\0';
}

char hal_console_getchar(void) {
    if (!hal_console_input_available()) {
        return '\n';
    }
    return g_input[g_input_pos++];
}

static bool contains(const char* haystack, const char* needle) {
    size_t needle_len = strlen(needle);
    if (!needle_len) {
        return true;
    }
    for (; *haystack; haystack++) {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static void dummy_entry(void* arg) {
    (void)arg;
}

static void dummy_signal_handler(int signal) {
    (void)signal;
}

int main(void) {
    vfs_init();
    scheduler_init();
    syscall_init();

    tty_t tty;
    tty_init(&tty);
    g_input =
        "echo hello world\n"
        "uname -a\n"
        "cat /proc/self/status\n"
        "ps\n"
        "syscalls\n"
        "exit 7\n";

    CHECK(userland_bootstrap(&tty) == 7);
    CHECK(process_count() == 1);
    CHECK(process_get(1) != NULL);
    CHECK(process_get(2) == NULL);
    CHECK(process_get(1)->state == PROCESS_STATE_RUNNING);
    CHECK(process_get_current() == process_get(1));

    CHECK(contains(g_output, "tach ("));
    CHECK(contains(g_output, "hello world"));
    CHECK(contains(g_output, "userspace"));
    CHECK(contains(g_output, "Pid:\t2"));
    CHECK(contains(g_output, "getpid"));

    CHECK(syscall_dispatch(SYS_GETPID, 0, 0, 0, 0, 0, 0) == 1);
    const char message[] = "syscall-write-ok\n";
    CHECK(syscall_dispatch(SYS_WRITE, 1, (uint64_t)(uintptr_t)message,
                           sizeof(message) - 1, 0, 0, 0) ==
          (long)(sizeof(message) - 1));
    CHECK(contains(g_output, "syscall-write-ok"));

    const char console_path[] = "/dev/tty";
    long fd = syscall_dispatch(SYS_OPEN, (uint64_t)(uintptr_t)console_path,
                               O_RDWR, 0, 0, 0, 0);
    CHECK(fd == 3);
    CHECK(syscall_dispatch(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0) == 0);

    struct process* worker = process_create("worker");
    CHECK(worker != NULL);
    CHECK(process_start(worker, (void*)dummy_entry, NULL) == 0);
    scheduler_yield();
    CHECK(process_get_current() == worker);

    sem_t semaphore;
    sem_init(&semaphore, 1);
    sem_acquire(&semaphore);
    CHECK(semaphore.value == 0);
    CHECK(semaphore.waiters == 0);
    sem_release(&semaphore);
    CHECK(semaphore.value == 1);

    sem_t blocking;
    sem_init(&blocking, 0);
    sem_acquire(&blocking);
    CHECK(worker->state == PROCESS_STATE_BLOCKED);
    CHECK(blocking.waiters == 1);
    sem_release(&blocking);
    CHECK(worker->state == PROCESS_STATE_RUNNING);
    CHECK(blocking.waiters == 0);

    process_set_current(process_get(1));
    long child_pid = syscall_dispatch(SYS_FORK, 0, 0, 0, 0, 0, 0);
    CHECK(child_pid > 0);
    struct process* child = process_get((pid_t)child_pid);
    CHECK(child != NULL);
    CHECK(child->ppid == 1);
    CHECK(fd_get(child->fds, 1) != NULL);
    CHECK(syscall_dispatch(SYS_KILL, (uint64_t)child_pid, SIGKILL,
                           0, 0, 0, 0) == 0);
    CHECK(child->state == PROCESS_STATE_ZOMBIE);
    int child_status = 0;
    CHECK(syscall_dispatch(SYS_WAITPID, (uint64_t)child_pid,
                           (uint64_t)(uintptr_t)&child_status,
                           0, 0, 0, 0) == child_pid);
    CHECK(child_status == 128 + SIGKILL);
    CHECK(process_get((pid_t)child_pid) == NULL);

    sigaction_t action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = dummy_signal_handler;
    CHECK(signal_set_action(process_get(1), SIGUSR1, &action, NULL) == 0);
    CHECK(signal_send(1, SIGUSR1) == 0);
    uintptr_t signal_stack[8];
    struct user_context signal_context;
    memset(&signal_context, 0, sizeof(signal_context));
    signal_context.pc = 0x1234;
    signal_context.sp = (uintptr_t)(signal_stack + 8);
    CHECK(signal_deliver_pending(process_get(1), &signal_context) == SIGUSR1);
    CHECK(signal_context.pc == (uintptr_t)dummy_signal_handler);
    CHECK(signal_context.regs[1] == SIGUSR1);
    CHECK(*(uintptr_t*)signal_context.sp == 0x1234);

    process_set_current(process_get(1));
    uint64_t before = process_get(1)->runtime_ticks;
    hal_timer_init();
    hal_timer_interrupt();
    CHECK(process_get(1)->runtime_ticks == before + 1);

    /* Anonymous pipes, dup/dup2, lseek, getppid/getuid. */
    process_set_current(process_get(1));
    int pipe_fds[2] = {-1, -1};
    CHECK(syscall_dispatch(SYS_PIPE, (uint64_t)(uintptr_t)pipe_fds,
                           0, 0, 0, 0, 0) == 0);
    CHECK(pipe_fds[0] >= 0 && pipe_fds[1] >= 0 && pipe_fds[0] != pipe_fds[1]);
    CHECK(syscall_dispatch(SYS_WRITE, (uint64_t)pipe_fds[0],
                           (uint64_t)(uintptr_t)"x", 1, 0, 0, 0) == -EBADF);
    CHECK(syscall_dispatch(SYS_WRITE, (uint64_t)pipe_fds[1],
                           (uint64_t)(uintptr_t)"hello", 5, 0, 0, 0) == 5);
    char pipe_buffer[16];
    CHECK(syscall_dispatch(SYS_READ, (uint64_t)pipe_fds[0],
                           (uint64_t)(uintptr_t)pipe_buffer,
                           sizeof(pipe_buffer), 0, 0, 0) == 5);
    CHECK(memcmp(pipe_buffer, "hello", 5) == 0);
    CHECK(syscall_dispatch(SYS_LSEEK, (uint64_t)pipe_fds[0], 0, SEEK_SET,
                           0, 0, 0) == -ESPIPE);

    /* The write end survives closing the original while a dup is open. */
    long dup_fd = syscall_dispatch(SYS_DUP, (uint64_t)pipe_fds[1],
                                   0, 0, 0, 0, 0);
    CHECK(dup_fd >= 0 && dup_fd != pipe_fds[1]);
    CHECK(syscall_dispatch(SYS_CLOSE, (uint64_t)pipe_fds[1],
                           0, 0, 0, 0, 0) == 0);
    CHECK(syscall_dispatch(SYS_WRITE, (uint64_t)dup_fd,
                           (uint64_t)(uintptr_t)"ok", 2, 0, 0, 0) == 2);
    CHECK(syscall_dispatch(SYS_READ, (uint64_t)pipe_fds[0],
                           (uint64_t)(uintptr_t)pipe_buffer,
                           sizeof(pipe_buffer), 0, 0, 0) == 2);
    CHECK(memcmp(pipe_buffer, "ok", 2) == 0);

    /* Closing the last write end turns an empty pipe into EOF. */
    CHECK(syscall_dispatch(SYS_CLOSE, (uint64_t)dup_fd, 0, 0, 0, 0, 0) == 0);
    CHECK(syscall_dispatch(SYS_READ, (uint64_t)pipe_fds[0],
                           (uint64_t)(uintptr_t)pipe_buffer,
                           sizeof(pipe_buffer), 0, 0, 0) == 0);
    CHECK(syscall_dispatch(SYS_CLOSE, (uint64_t)pipe_fds[0],
                           0, 0, 0, 0, 0) == 0);
    CHECK(syscall_dispatch(SYS_PIPE, 0, 0, 0, 0, 0, 0) == -EFAULT);

    /* lseek on a regular (memory-backed) file. */
    CHECK(vfs_register_memfile("/pipe-seek.txt", "abcdef", 6) == 0);
    long seek_fd = syscall_dispatch(SYS_OPEN,
                                    (uint64_t)(uintptr_t)"/pipe-seek.txt",
                                    O_RDONLY, 0, 0, 0, 0);
    CHECK(seek_fd >= 0);
    CHECK(syscall_dispatch(SYS_LSEEK, (uint64_t)seek_fd, 2, SEEK_SET,
                           0, 0, 0) == 2);
    CHECK(syscall_dispatch(SYS_READ, (uint64_t)seek_fd,
                           (uint64_t)(uintptr_t)pipe_buffer, 2, 0, 0, 0) == 2);
    CHECK(memcmp(pipe_buffer, "cd", 2) == 0);
    CHECK(syscall_dispatch(SYS_LSEEK, (uint64_t)seek_fd, (uint64_t)(int64_t)-1,
                           SEEK_END, 0, 0, 0) == 5);
    CHECK(syscall_dispatch(SYS_READ, (uint64_t)seek_fd,
                           (uint64_t)(uintptr_t)pipe_buffer, 4, 0, 0, 0) == 1);
    CHECK(pipe_buffer[0] == 'f');
    CHECK(syscall_dispatch(SYS_LSEEK, (uint64_t)seek_fd, (uint64_t)(int64_t)-9,
                           SEEK_CUR, 0, 0, 0) == -EINVAL);
    CHECK(syscall_dispatch(SYS_LSEEK, (uint64_t)seek_fd, 0, 99,
                           0, 0, 0) == -EINVAL);

    /* dup2 onto a chosen slot, then identity queries. */
    CHECK(syscall_dispatch(SYS_DUP2, (uint64_t)seek_fd, 20, 0, 0, 0, 0) == 20);
    CHECK(fd_get(process_get(1)->fds, 20) != NULL);
    CHECK(syscall_dispatch(SYS_DUP2, 999, 21, 0, 0, 0, 0) == -EBADF);
    CHECK(syscall_dispatch(SYS_CLOSE, 20, 0, 0, 0, 0, 0) == 0);
    CHECK(syscall_dispatch(SYS_CLOSE, (uint64_t)seek_fd, 0, 0, 0, 0, 0) == 0);
    CHECK(syscall_dispatch(SYS_GETPPID, 0, 0, 0, 0, 0, 0) ==
          (long)process_get(1)->ppid);
    CHECK(syscall_dispatch(SYS_GETUID, 0, 0, 0, 0, 0, 0) == 0);

    vterm_t vt;
    vterm_init(&vt);
    vterm_putchar(&vt, 'O');
    vterm_putchar(&vt, 'K');
    CHECK((char)(vt.buffer[0] & 0xFF) == 'O');
    CHECK((char)(vt.buffer[1] & 0xFF) == 'K');

    return 0;
}
