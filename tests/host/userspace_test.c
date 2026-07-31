/*
 * Host-side integration test for the embedded userspace foundation.
 * This deliberately replaces the hardware console with deterministic
 * memory-backed input/output.
 */
#include <kernel/string.h>
#include <fs/vfs.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>
#include <ipc/sem.h>
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
    CHECK(process_count() == 2);
    CHECK(process_get(1) != NULL);
    CHECK(process_get(2) != NULL);
    CHECK(process_get(1)->state == PROCESS_STATE_RUNNING);
    CHECK(process_get(2)->state == PROCESS_STATE_ZOMBIE);
    CHECK(process_get_current() == process_get(1));

    CHECK(contains(g_output, "tach interactive shell"));
    CHECK(contains(g_output, "hello world"));
    CHECK(contains(g_output, "embedded-userspace"));
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

    vterm_t vt;
    vterm_init(&vt);
    vterm_putchar(&vt, 'O');
    vterm_putchar(&vt, 'K');
    CHECK((char)(vt.buffer[0] & 0xFF) == 'O');
    CHECK((char)(vt.buffer[1] & 0xFF) == 'K');

    return 0;
}
