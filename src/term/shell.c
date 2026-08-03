#include <kernel/errno.h>
#include <kernel/klog.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/version.h>
#include <term/shell.h>
#include <hal/console.h>
#include <proc/process.h>
#include <proc/syscall.h>
#include <fs/vfs.h>
#include <mm/pmm.h>

#define SHELL_MAX_ARGS 16

static shell_t* g_active_shell;

static void shell_putc(char c) {
    hal_console_putchar(c);
}

#define shell_printf(...) kprintf_to(shell_putc, __VA_ARGS__)

static const char* shell_arch(void) {
#if defined(__x86_64__)
    return "x86_64";
#elif defined(__i386__)
    return "i686";
#elif defined(__aarch64__)
    return "aarch64";
#elif defined(__arm__)
    return "arm32";
#elif defined(__riscv) && __riscv_xlen == 64
    return "riscv64";
#elif defined(__riscv)
    return "riscv32";
#else
    return "unknown";
#endif
}

static int parse_line(char* line, const char** argv, int capacity) {
    int argc = 0;
    char* cursor = line;
    while (*cursor && argc < capacity) {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (!*cursor) {
            break;
        }

        char quote = 0;
        if (*cursor == '\'' || *cursor == '"') {
            quote = *cursor++;
        }
        argv[argc++] = cursor;

        char* out = cursor;
        while (*cursor) {
            if (quote) {
                if (*cursor == quote) {
                    cursor++;
                    break;
                }
            } else if (*cursor == ' ' || *cursor == '\t') {
                break;
            }
            if (*cursor == '\\' && cursor[1]) {
                cursor++;
            }
            *out++ = *cursor++;
        }
        bool had_separator = !quote && *cursor != '\0';
        *out = '\0';
        if (had_separator) {
            cursor++;
        }
        while (*cursor == ' ' || *cursor == '\t') {
            *cursor++ = '\0';
        }
    }
    return argc;
}

static void command_help(void) {
    shell_printf(
        "Built-in commands:\n"
        "  help                 show this command list\n"
        "  clear                clear the display\n"
        "  echo [args...]       print arguments\n"
        "  uname [-a]           show system information\n"
        "  ps                   list processes\n"
        "  id | whoami          show current identity\n"
        "  pwd | cd [path]      inspect or change logical directory\n"
        "  ls [path]            list the embedded root filesystem\n"
        "  cat <proc-file>      read supported /proc files\n"
        "  history              show command history\n"
        "  syscalls             show active syscall ABI entries\n"
        "  status               show the previous command status\n"
        "  exit [status]        leave the shell\n"
        "  /path | command      run an ELF from the initrd (/bin searched)\n"
        "\n"
        "Editing: Backspace, Ctrl+U, Ctrl+W, Up/Down history.\n");
}

static int parse_small_int(const char* text, int fallback) {
    if (!text || !*text) {
        return fallback;
    }
    int value = 0;
    bool negative = false;
    if (*text == '-') {
        negative = true;
        text++;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text++ - '0');
    }
    return negative ? -value : value;
}

static void command_ps(void) {
    struct process* current = process_get_current();
    shell_printf(" PID  PPID STATE     TICKS NAME\n");
    for (size_t i = 0; i < process_count(); i++) {
        struct process* proc = process_at(i);
        if (!proc) {
            continue;
        }
        shell_printf("%c%4u %5u ",
                     proc == current ? '*' : ' ',
                     (unsigned)proc->pid,
                     (unsigned)proc->ppid);
        shell_printf("%s", process_state_name(proc->state));
        size_t state_len = strlen(process_state_name(proc->state));
        while (state_len++ < 9) {
            shell_putc(' ');
        }
        shell_printf("%5lu %s\n", proc->runtime_ticks, proc->name);
    }
}

static void command_history(tty_t* tty) {
    if (!tty || !tty->history_count) {
        shell_printf("No history yet.\n");
        return;
    }
    size_t oldest = tty->history_count < TTY_HISTORY_SIZE ? 0 : tty->history_head;
    for (size_t i = 0; i < tty->history_count; i++) {
        size_t slot = (oldest + i) % TTY_HISTORY_SIZE;
        shell_printf("%2u  %s\n", (unsigned)(i + 1), tty->history[slot]);
    }
}

static int command_ls(const char* path) {
    if (!path || strcmp(path, "/") == 0) {
        shell_printf("dev/\nproc/\nsbin/\n");
        return 0;
    }
    if (strcmp(path, "/dev") == 0) {
        shell_printf("console\ntty\n");
        return 0;
    }
    if (strcmp(path, "/proc") == 0) {
        shell_printf("version\nmeminfo\nself/\n");
        for (size_t i = 0; i < process_count(); i++) {
            struct process* proc = process_at(i);
            if (proc) {
                shell_printf("%u/\n", (unsigned)proc->pid);
            }
        }
        return 0;
    }
    if (strcmp(path, "/sbin") == 0) {
        shell_printf("init\n");
        return 0;
    }
    shell_printf("ls: %s: no such directory\n", path);
    return -ENOENT;
}

static int command_cat(const char* path) {
    if (!path) {
        shell_printf("cat: missing operand\n");
        return -EINVAL;
    }
    if (strcmp(path, "/proc/version") == 0) {
        shell_printf("tach %s %s (freestanding C kernel)\n",
                     TACH_VERSION_STRING, shell_arch());
        return 0;
    }
    if (strcmp(path, "/proc/meminfo") == 0) {
        shell_printf("MemTotal:\t%lu kB\nMemFree:\t%lu kB\n",
                     (unsigned long)(pmm_get_total_pages() * 4),
                     (unsigned long)(pmm_get_free_pages() * 4));
        return 0;
    }
    if (strcmp(path, "/proc/self/status") == 0) {
        struct process* proc = process_get_current();
        if (!proc) {
            return -ESRCH;
        }
        shell_printf("Name:\t%s\nPid:\t%u\nPPid:\t%u\nState:\t%s\nUid:\t%u\nGid:\t%u\n",
                     proc->name, (unsigned)proc->pid, (unsigned)proc->ppid,
                     process_state_name(proc->state), (unsigned)proc->uid,
                     (unsigned)proc->gid);
        return 0;
    }
    shell_printf("cat: %s: no such file\n", path);
    return -ENOENT;
}

void shell_init(shell_t* sh, const char* prompt) {
    if (!sh) {
        return;
    }
    memset(sh, 0, sizeof(*sh));
    sh->prompt = prompt ? prompt : "tach";
    sh->running = true;
    strcpy(sh->cwd, "/");
}

int shell_exec(const char* cmd, int argc, const char** argv) {
    if (!cmd || !*cmd || !g_active_shell) {
        return 0;
    }
    shell_t* sh = g_active_shell;

    if (strcmp(cmd, "help") == 0) {
        command_help();
    } else if (strcmp(cmd, "clear") == 0) {
        hal_console_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) shell_putc(' ');
            shell_printf("%s", argv[i]);
        }
        shell_putc('\n');
    } else if (strcmp(cmd, "uname") == 0) {
        if (argc > 1 && strcmp(argv[1], "-a") == 0) {
            shell_printf("tach %s %s userspace\n",
                         TACH_VERSION_STRING, shell_arch());
        } else {
            shell_printf("tach\n");
        }
    } else if (strcmp(cmd, "ps") == 0) {
        command_ps();
    } else if (strcmp(cmd, "id") == 0) {
        struct process* proc = process_get_current();
        shell_printf("uid=%u(root) gid=%u(root) pid=%u\n",
                     proc ? (unsigned)proc->uid : 0,
                     proc ? (unsigned)proc->gid : 0,
                     proc ? (unsigned)proc->pid : 0);
    } else if (strcmp(cmd, "whoami") == 0) {
        shell_printf("root\n");
    } else if (strcmp(cmd, "pwd") == 0) {
        shell_printf("%s\n", sh->cwd);
    } else if (strcmp(cmd, "cd") == 0) {
        const char* path = argc > 1 ? argv[1] : "/";
        if (strcmp(path, "/") != 0 && strcmp(path, "/dev") != 0 &&
            strcmp(path, "/proc") != 0 && strcmp(path, "/sbin") != 0) {
            shell_printf("cd: %s: no such directory\n", path);
            return -ENOENT;
        }
        strncpy(sh->cwd, path, sizeof(sh->cwd) - 1);
        sh->cwd[sizeof(sh->cwd) - 1] = '\0';
    } else if (strcmp(cmd, "ls") == 0) {
        return command_ls(argc > 1 ? argv[1] : sh->cwd);
    } else if (strcmp(cmd, "cat") == 0) {
        return command_cat(argc > 1 ? argv[1] : NULL);
    } else if (strcmp(cmd, "history") == 0) {
        command_history(sh->tty);
    } else if (strcmp(cmd, "syscalls") == 0) {
        for (int i = 0; i < SYSCALL_TABLE_SIZE; i++) {
            const char* name = syscall_name(i);
            if (name) {
                shell_printf("%3d  %s\n", i, name);
            }
        }
    } else if (strcmp(cmd, "status") == 0) {
        shell_printf("%d\n", sh->last_status);
    } else if (strcmp(cmd, "true") == 0) {
        return 0;
    } else if (strcmp(cmd, "false") == 0) {
        return 1;
    } else if (strcmp(cmd, "exit") == 0) {
        int status = argc > 1 ? parse_small_int(argv[1], 0) : 0;
        sh->running = false;
        process_exit(status);
        return status;
    } else {
        char path[128];
        const char* executable = cmd;
        if (cmd[0] != '/') {
            strcpy(path, "/bin/");
            if (strlen(cmd) + strlen(path) >= sizeof(path)) {
                shell_printf("%s: name too long\n", cmd);
                return -ENAMETOOLONG;
            }
            strcpy(path + strlen(path), cmd);
            executable = path;
        }
        const void* image;
        size_t image_size;
        if (vfs_read_file(executable, &image, &image_size) == 0) {
            struct process* child = process_create(executable);
            if (!child) return -ENOMEM;
            int result = process_exec_image(child, image, image_size,
                                            executable);
            if (result < 0) {
                process_destroy(child);
                shell_printf("%s: cannot execute (%d)\n", executable,
                             result);
                return result;
            }
            int status = 0;
            pid_t waited = process_waitpid(child->pid, &status, 0);
            return (int32_t)waited < 0 ? (int32_t)waited : status;
        }
        shell_printf("%s: command not found\n", cmd);
        return -ENOENT;
    }
    return 0;
}

void shell_run(shell_t* sh) {
    if (!sh || !sh->tty) {
        return;
    }
    g_active_shell = sh;
    shell_printf("tach (%s)\n", TACH_VERSION_STRING);
    shell_printf("Type 'help' for available commands.\n\n");

    char line[TTY_BUFFER_SIZE];
    const char* argv[SHELL_MAX_ARGS];
    while (sh->running) {
        shell_printf("%s:%s# ", sh->prompt, sh->cwd);
        ssize_t length = tty_readline(sh->tty, line, sizeof(line));
        if (length <= 0) {
            continue;
        }
        int argc = parse_line(line, argv, SHELL_MAX_ARGS);
        if (argc) {
            sh->last_status = shell_exec(argv[0], argc, argv);
        }
    }
    g_active_shell = NULL;
}
