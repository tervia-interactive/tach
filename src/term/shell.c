#include <kernel/errno.h>
#include <kernel/klog.h>
#include <kernel/printf.h>
#include <kernel/string.h>
#include <kernel/version.h>
#include <term/shell.h>
#include <hal/console.h>
#include <hal/time.h>
#include <proc/process.h>
#include <proc/syscall.h>
#include <fs/vfs.h>
#include <mm/pmm.h>

#define SHELL_MAX_ARGS 16
#define SHELL_PATH_MAX 128
#define SHELL_MAX_CANDIDATES 32
#define SHELL_NOT_FOUND 127

static shell_t* g_active_shell;

static void shell_putc(char c) {
    hal_console_putchar(c);
}

#define shell_printf(...) kprintf_to(shell_putc, __VA_ARGS__)

static const char* const g_builtins[] = {
    "cat", "cd", "clear", "echo", "exit", "false", "free", "help", "history",
    "hostname", "id", "ls", "mkdir", "mv", "ps", "pwd", "rm", "rmdir",
    "sleep", "stat", "status", "syscalls", "touch", "true", "uname", "uptime",
    "whoami",
};
#define SHELL_BUILTIN_COUNT (sizeof(g_builtins) / sizeof(g_builtins[0]))

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

/* Split `line` in place into words. Handles 'single' and "double" quotes
 * (also glued to other text, as in a"b"c) and backslash escapes. */
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

        argv[argc++] = cursor;
        char* out = cursor;
        char quote = 0;
        while (*cursor) {
            char ch = *cursor;
            if (quote) {
                if (ch == quote) {
                    quote = 0;
                    cursor++;
                    continue;
                }
                if (ch == '\\' && quote == '"' && cursor[1]) {
                    cursor++;
                }
                *out++ = *cursor++;
                continue;
            }
            if (ch == ' ' || ch == '\t') {
                break;
            }
            if (ch == '\'' || ch == '"') {
                quote = ch;
                cursor++;
                continue;
            }
            if (ch == '\\' && cursor[1]) {
                cursor++;
            }
            *out++ = *cursor++;
        }
        bool more = *cursor != '\0';
        *out = '\0';
        if (more) {
            cursor++;
        }
    }
    return argc;
}

static void command_help(void) {
    shell_printf(
        "Built-in commands:\n"
        "  help                 show this command list\n"
        "  clear                clear the display (Ctrl+L)\n"
        "  echo [-n] [args] [> file | >> file]   print arguments\n"
        "  uname [-a]           show system information\n"
        "  hostname | uptime    machine name / time since boot\n"
        "  ps | free            list processes / memory usage\n"
        "  id | whoami          show current identity\n"
        "  pwd | cd [path|-]    show or change directory (supports .. and .)\n"
        "  ls [-l] [path...]    list directories\n"
        "  cat <file...>        print files (/proc/* supported)\n"
        "  stat <path>          show type and size\n"
        "  mkdir|rmdir|rm|touch|mv   manage files (writable mounts, e.g. /tmp)\n"
        "  sleep <seconds>      wait\n"
        "  history | status     command history / previous status ($?)\n"
        "  syscalls             show active syscall ABI entries\n"
        "  exit [status]        leave the shell\n"
        "  /path | command      run an ELF from the initrd (/bin searched)\n"
        "\n"
        "Chain commands with ;  &&  ||   Quote with '...' or \"...\".\n"
        "Editing: Left/Right, Home/End, Delete, Backspace, Tab completion,\n"
        "  Ctrl+A/E/K/U/W, Ctrl+C cancel, Ctrl+L clear, Up/Down history.\n");
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

static int shell_resolve(const shell_t* sh, const char* arg, char* out,
                         size_t capacity) {
    int rc = vfs_resolve_path(sh->cwd, arg, out, capacity);
    if (rc < 0) {
        return rc;
    }
    return 0;
}

static bool shell_is_dir(const char* abs_path) {
    struct tach_stat info;
    if (vfs_stat(abs_path, &info) != 0) {
        return false;
    }
    return TACH_S_ISDIR(info.st_mode);
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

/* Legacy listing used when the VFS cannot enumerate a synthetic directory. */
static int command_ls_fallback(const char* path) {
    if (strcmp(path, "/") == 0) {
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
    return -ENOENT;
}

static char shell_type_char(uint8_t type) {
    switch (type) {
    case VNODE_DIR:   return 'd';
    case VNODE_CHAR:  return 'c';
    case VNODE_BLOCK: return 'b';
    case VNODE_LINK:  return 'l';
    case VNODE_PIPE:  return 'p';
    default:          return '-';
    }
}

static int ls_one(shell_t* sh, const char* arg, bool longfmt, bool header) {
    char path[SHELL_PATH_MAX];
    if (shell_resolve(sh, arg, path, sizeof(path)) < 0) {
        shell_printf("ls: %s: bad path\n", arg);
        return -ENOENT;
    }
    struct tach_stat info;
    if (vfs_stat(path, &info) == 0 && !TACH_S_ISDIR(info.st_mode)) {
        if (longfmt) {
            shell_printf("%c %8lu ", TACH_S_ISCHR(info.st_mode) ? 'c' : '-',
                         (unsigned long)info.st_size);
        }
        shell_printf("%s\n", arg);
        return 0;
    }
    int error = 0;
    struct vnode* dir = vfs_open_err(path, O_RDONLY, &error);
    if (!dir) {
        if (command_ls_fallback(path) == 0) {
            return 0;
        }
        shell_printf("ls: %s: no such directory\n", arg);
        return error < 0 ? error : -ENOENT;
    }
    if (header) {
        shell_printf("%s:\n", path);
    }
    struct dirent entry;
    size_t count = 0;
    for (size_t index = 0;; index++) {
        int rc = vfs_readdir(dir, index, &entry);
        if (rc < 0) {
            if (index == 0 && command_ls_fallback(path) == 0) {
                count = 1;
            } else if (index == 0) {
                shell_printf("ls: %s: %d\n", arg, rc);
            }
            break;
        }
        if (rc == 0) {
            break;
        }
        count++;
        if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) {
            continue;
        }
        if (longfmt) {
            char child[SHELL_PATH_MAX];
            struct tach_stat child_info;
            unsigned long size = 0;
            size_t base = strlen(path);
            if (base + strlen(entry.d_name) + 2 < sizeof(child)) {
                strcpy(child, path);
                if (base > 1) {
                    child[base++] = '/';
                }
                strcpy(child + base, entry.d_name);
                if (vfs_stat(child, &child_info) == 0) {
                    size = (unsigned long)child_info.st_size;
                }
            }
            shell_printf("%c %8lu ", shell_type_char(entry.d_type), size);
        }
        shell_printf("%s%s\n", entry.d_name, entry.d_type == VNODE_DIR ? "/" : "");
    }
    (void)count;
    vfs_release(dir);
    return 0;
}

static int command_ls(shell_t* sh, int argc, const char** argv) {
    bool longfmt = false;
    int first = 1;
    while (first < argc && argv[first][0] == '-' && argv[first][1]) {
        for (const char* f = argv[first] + 1; *f; f++) {
            if (*f == 'l') longfmt = true;
        }
        first++;
    }
    if (first >= argc) {
        return ls_one(sh, ".", longfmt, false);
    }
    int status = 0;
    bool header = (argc - first) > 1;
    for (int i = first; i < argc; i++) {
        int rc = ls_one(sh, argv[i], longfmt, header);
        if (rc) status = rc;
        if (header && i + 1 < argc) shell_putc('\n');
    }
    return status;
}

static int cat_proc(const char* path) {
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
    return 1; /* not a built-in /proc file */
}

static int cat_one(shell_t* sh, const char* arg) {
    char path[SHELL_PATH_MAX];
    if (shell_resolve(sh, arg, path, sizeof(path)) < 0) {
        shell_printf("cat: %s: bad path\n", arg);
        return -ENOENT;
    }
    int proc_rc = cat_proc(path);
    if (proc_rc <= 0) {
        return proc_rc;
    }
    int error = 0;
    struct vnode* vn = vfs_open_err(path, O_RDONLY, &error);
    if (!vn) {
        shell_printf("cat: %s: no such file\n", arg);
        return error < 0 ? error : -ENOENT;
    }
    if (vn->type == VNODE_DIR) {
        vfs_release(vn);
        shell_printf("cat: %s: is a directory\n", arg);
        return -EISDIR;
    }
    char chunk[128];
    int64_t offset = 0;
    bool last_newline = true;
    while (1) {
        ssize_t got = vfs_read(vn, chunk, sizeof(chunk), offset);
        if (got <= 0) {
            break;
        }
        for (ssize_t i = 0; i < got; i++) {
            char c = chunk[i];
            if (c == '\n' || c == '\t' || (c >= ' ' && c <= '~')) {
                shell_putc(c);
            } else {
                shell_putc('.');
            }
        }
        last_newline = chunk[got - 1] == '\n';
        offset += got;
    }
    if (!last_newline) {
        shell_putc('\n');
    }
    vfs_release(vn);
    return 0;
}

static int command_cat(shell_t* sh, int argc, const char** argv) {
    if (argc < 2) {
        shell_printf("cat: missing operand\n");
        return -EINVAL;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        int rc = cat_one(sh, argv[i]);
        if (rc) status = rc;
    }
    return status;
}

static int command_cd(shell_t* sh, const char* arg) {
    char target[SHELL_PATH_MAX];
    if (arg && strcmp(arg, "-") == 0) {
        arg = sh->oldpwd[0] ? sh->oldpwd : sh->cwd;
        shell_printf("%s\n", arg);
    }
    if (shell_resolve(sh, arg ? arg : "/", target, sizeof(target)) < 0 ||
        !shell_is_dir(target)) {
        shell_printf("cd: %s: no such directory\n", arg ? arg : "/");
        return -ENOENT;
    }
    strcpy(sh->oldpwd, sh->cwd);
    strcpy(sh->cwd, target);
    return 0;
}

/* Run `fn` for every path argument; used by rm/rmdir/mkdir. */
static int for_each_path(shell_t* sh, const char* name, int argc,
                         const char** argv, int (*fn)(const char*)) {
    if (argc < 2) {
        shell_printf("%s: missing operand\n", name);
        return -EINVAL;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        char path[SHELL_PATH_MAX];
        if (shell_resolve(sh, argv[i], path, sizeof(path)) < 0) {
            shell_printf("%s: %s: bad path\n", name, argv[i]);
            status = -ENOENT;
            continue;
        }
        int rc = fn(path);
        if (rc < 0) {
            shell_printf("%s: %s: failed (%d)\n", name, argv[i], rc);
            status = rc;
        }
    }
    return status;
}

static int touch_path(const char* path) {
    int error = 0;
    struct vnode* vn = vfs_open_err(path, O_WRONLY | O_CREAT, &error);
    if (!vn) {
        return error < 0 ? error : -EIO;
    }
    vfs_release(vn);
    return 0;
}

static int command_stat(shell_t* sh, int argc, const char** argv) {
    if (argc < 2) {
        shell_printf("stat: missing operand\n");
        return -EINVAL;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        char path[SHELL_PATH_MAX];
        struct tach_stat info;
        if (shell_resolve(sh, argv[i], path, sizeof(path)) < 0 ||
            vfs_stat(path, &info) != 0) {
            shell_printf("stat: %s: no such file or directory\n", argv[i]);
            status = -ENOENT;
            continue;
        }
        const char* kind = TACH_S_ISDIR(info.st_mode) ? "directory" :
                           TACH_S_ISCHR(info.st_mode) ? "character device" :
                           TACH_S_ISREG(info.st_mode) ? "regular file" : "other";
        shell_printf("  File: %s\n  Type: %s\n  Size: %lu\n", path, kind,
                     (unsigned long)info.st_size);
    }
    return status;
}

static int command_mv(shell_t* sh, int argc, const char** argv) {
    if (argc != 3) {
        shell_printf("mv: usage: mv <from> <to>\n");
        return -EINVAL;
    }
    char from[SHELL_PATH_MAX];
    char to[SHELL_PATH_MAX];
    if (shell_resolve(sh, argv[1], from, sizeof(from)) < 0 ||
        shell_resolve(sh, argv[2], to, sizeof(to)) < 0) {
        shell_printf("mv: bad path\n");
        return -ENOENT;
    }
    int rc = vfs_rename(from, to);
    if (rc < 0) {
        shell_printf("mv: failed (%d)\n", rc);
    }
    return rc;
}

static void command_uptime(void) {
    uint64_t freq = hal_timer_get_frequency();
    uint64_t ticks = hal_timer_get_ticks();
    unsigned long seconds = freq ? (unsigned long)(ticks / freq) : 0;
    shell_printf("up %lu:%02lu:%02lu (%lu ticks)\n", seconds / 3600,
                 (seconds / 60) % 60, seconds % 60, (unsigned long)ticks);
}

static void command_free(void) {
    unsigned long total = (unsigned long)(pmm_get_total_pages() * 4);
    unsigned long avail = (unsigned long)(pmm_get_free_pages() * 4);
    shell_printf("        total     used     free  (kB)\n");
    shell_printf("Mem: %8lu %8lu %8lu\n", total, total - avail, avail);
}

/* echo with optional "> file" / ">> file" redirection. */
static int command_echo(shell_t* sh, int argc, const char** argv) {
    int end = argc;
    int redirect = 0;
    const char* target = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            redirect = argv[i][1] ? 2 : 1;
            end = i;
            if (i + 1 >= argc) {
                shell_printf("echo: missing file after redirection\n");
                return -EINVAL;
            }
            target = argv[i + 1];
            break;
        }
    }
    int first = 1;
    bool newline = true;
    if (first < end && strcmp(argv[first], "-n") == 0) {
        newline = false;
        first++;
    }

    char text[TTY_BUFFER_SIZE + 2];
    size_t length = 0;
    for (int i = first; i < end; i++) {
        size_t part = strlen(argv[i]);
        if (length + part + 2 >= sizeof(text)) {
            break;
        }
        if (i > first) text[length++] = ' ';
        memcpy(text + length, argv[i], part);
        length += part;
    }
    if (newline) text[length++] = '\n';

    if (!redirect) {
        hal_console_write(text, length);
        return 0;
    }
    char path[SHELL_PATH_MAX];
    if (shell_resolve(sh, target, path, sizeof(path)) < 0) {
        shell_printf("echo: %s: bad path\n", target);
        return -ENOENT;
    }
    int error = 0;
    int flags = O_WRONLY | O_CREAT | (redirect == 2 ? O_APPEND : O_TRUNC);
    struct vnode* vn = vfs_open_err(path, flags, &error);
    if (!vn) {
        shell_printf("echo: %s: cannot open for writing (%d)\n", target, error);
        return error < 0 ? error : -EIO;
    }
    int64_t offset = redirect == 2 ? (int64_t)vn->size : 0;
    ssize_t written = vfs_write(vn, text, length, offset);
    vfs_release(vn);
    if (written < 0 || (size_t)written != length) {
        shell_printf("echo: %s: write failed\n", target);
        return written < 0 ? (int)written : -EIO;
    }
    return 0;
}

static void shell_print_prompt(void) {
    if (g_active_shell) {
        shell_printf("%s:%s# ", g_active_shell->prompt, g_active_shell->cwd);
    }
}

/* ---- Tab completion ------------------------------------------------- */

typedef struct {
    char name[64];
    bool dir;
} candidate_t;

static void candidate_add(candidate_t* list, size_t* count, const char* name,
                          bool dir) {
    if (*count >= SHELL_MAX_CANDIDATES || strlen(name) >= sizeof(list[0].name)) {
        return;
    }
    for (size_t i = 0; i < *count; i++) {
        if (strcmp(list[i].name, name) == 0) {
            return;
        }
    }
    strcpy(list[*count].name, name);
    list[*count].dir = dir;
    (*count)++;
}

static void collect_dir(const char* dir_path, const char* prefix,
                        candidate_t* list, size_t* count) {
    int error = 0;
    struct vnode* dir = vfs_open_err(dir_path, O_RDONLY, &error);
    if (!dir) {
        return;
    }
    size_t plen = strlen(prefix);
    struct dirent entry;
    for (size_t index = 0;; index++) {
        if (vfs_readdir(dir, index, &entry) <= 0) {
            break;
        }
        if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0) {
            continue;
        }
        if (strncmp(entry.d_name, prefix, plen) == 0) {
            candidate_add(list, count, entry.d_name, entry.d_type == VNODE_DIR);
        }
    }
    vfs_release(dir);
}

static size_t shell_complete(tty_t* tty, char* out, size_t out_size) {
    shell_t* sh = g_active_shell;
    if (!sh || out_size < 2) {
        return 0;
    }
    size_t end = tty->cursor;
    size_t start = end;
    while (start && tty->buffer[start - 1] != ' ') {
        start--;
    }
    bool first_word = true;
    for (size_t i = 0; i < start; i++) {
        if (tty->buffer[i] != ' ') {
            first_word = false;
            break;
        }
    }
    char word[SHELL_PATH_MAX];
    size_t wlen = end - start;
    if (wlen >= sizeof(word)) {
        return 0;
    }
    memcpy(word, tty->buffer + start, wlen);
    word[wlen] = '\0';

    candidate_t list[SHELL_MAX_CANDIDATES];
    size_t count = 0;
    const char* prefix = word;
    const char* slash = NULL;
    for (const char* p = word; *p; p++) {
        if (*p == '/') slash = p;
    }

    if (first_word && !slash) {
        for (size_t i = 0; i < SHELL_BUILTIN_COUNT; i++) {
            if (strncmp(g_builtins[i], word, wlen) == 0) {
                candidate_add(list, &count, g_builtins[i], false);
            }
        }
        collect_dir("/bin", word, list, &count);
    } else {
        char dir_arg[SHELL_PATH_MAX];
        char dir_abs[SHELL_PATH_MAX];
        if (slash) {
            size_t dlen = (size_t)(slash - word) + 1;
            memcpy(dir_arg, word, dlen);
            dir_arg[dlen] = '\0';
            prefix = slash + 1;
        } else {
            strcpy(dir_arg, ".");
        }
        if (shell_resolve(sh, dir_arg, dir_abs, sizeof(dir_abs)) < 0) {
            return 0;
        }
        collect_dir(dir_abs, prefix, list, &count);
    }
    if (!count) {
        return 0;
    }

    size_t plen = strlen(prefix);
    size_t common = strlen(list[0].name);
    for (size_t i = 1; i < count; i++) {
        size_t k = 0;
        while (k < common && list[i].name[k] == list[0].name[k]) {
            k++;
        }
        common = k;
    }
    size_t produced = 0;
    for (size_t i = plen; i < common && produced + 2 < out_size; i++) {
        out[produced++] = list[0].name[i];
    }
    if (count == 1) {
        out[produced++] = list[0].dir ? '/' : ' ';
        out[produced] = '\0';
        return produced;
    }
    out[produced] = '\0';
    if (produced == 0) {
        shell_putc('\n');
        for (size_t i = 0; i < count; i++) {
            shell_printf("%s%s  ", list[i].name, list[i].dir ? "/" : "");
        }
        shell_putc('\n');
        tty->complete_listed = true;
    }
    return produced;
}

/* ---- Core ------------------------------------------------------------ */

void shell_init(shell_t* sh, const char* prompt) {
    if (!sh) {
        return;
    }
    memset(sh, 0, sizeof(*sh));
    sh->prompt = prompt ? prompt : "tach";
    sh->running = true;
    strcpy(sh->cwd, "/");
    strcpy(sh->oldpwd, "/");
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
        return command_echo(sh, argc, argv);
    } else if (strcmp(cmd, "uname") == 0) {
        if (argc > 1 && strcmp(argv[1], "-a") == 0) {
            shell_printf("tach %s %s userspace\n",
                         TACH_VERSION_STRING, shell_arch());
        } else {
            shell_printf("tach\n");
        }
    } else if (strcmp(cmd, "hostname") == 0) {
        shell_printf("%s\n", sh->prompt);
    } else if (strcmp(cmd, "uptime") == 0) {
        command_uptime();
    } else if (strcmp(cmd, "free") == 0) {
        command_free();
    } else if (strcmp(cmd, "sleep") == 0) {
        int seconds = argc > 1 ? parse_small_int(argv[1], 0) : 0;
        if (argc < 2 || seconds < 0) {
            shell_printf("sleep: usage: sleep <seconds>\n");
            return -EINVAL;
        }
        hal_timer_sleep((uint64_t)seconds * 1000u);
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
        return command_cd(sh, argc > 1 ? argv[1] : NULL);
    } else if (strcmp(cmd, "ls") == 0) {
        return command_ls(sh, argc, argv);
    } else if (strcmp(cmd, "cat") == 0) {
        return command_cat(sh, argc, argv);
    } else if (strcmp(cmd, "stat") == 0) {
        return command_stat(sh, argc, argv);
    } else if (strcmp(cmd, "mkdir") == 0) {
        return for_each_path(sh, "mkdir", argc, argv, vfs_mkdir);
    } else if (strcmp(cmd, "rmdir") == 0) {
        return for_each_path(sh, "rmdir", argc, argv, vfs_rmdir);
    } else if (strcmp(cmd, "rm") == 0) {
        return for_each_path(sh, "rm", argc, argv, vfs_unlink);
    } else if (strcmp(cmd, "touch") == 0) {
        return for_each_path(sh, "touch", argc, argv, touch_path);
    } else if (strcmp(cmd, "mv") == 0) {
        return command_mv(sh, argc, argv);
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
        char path[SHELL_PATH_MAX];
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
        return SHELL_NOT_FOUND;
    }
    return 0;
}

static void format_int(char* out, int value) {
    char digits[12];
    size_t n = 0;
    unsigned magnitude = value < 0 ? 0u - (unsigned)value : (unsigned)value;
    do {
        digits[n++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude && n < sizeof(digits));
    size_t pos = 0;
    if (value < 0) out[pos++] = '-';
    while (n) out[pos++] = digits[--n];
    out[pos] = '\0';
}

/* Expand $? (outside single quotes) into `out`. */
static void expand_status(const char* in, char* out, size_t capacity,
                          int status) {
    char number[12];
    format_int(number, status);
    size_t pos = 0;
    char quote = 0;
    for (; *in && pos + 1 < capacity; in++) {
        if (quote == '\'') {
            if (*in == '\'') quote = 0;
        } else if (*in == '\'' || *in == '"') {
            if (!quote) quote = *in; else if (quote == *in) quote = 0;
        } else if (*in == '\\' && in[1]) {
            out[pos++] = *in++;
        } else if (in[0] == '$' && in[1] == '?') {
            for (const char* d = number; *d && pos + 1 < capacity; d++) {
                out[pos++] = *d;
            }
            in++;
            continue;
        }
        out[pos++] = *in;
    }
    out[pos] = '\0';
}

/* Run one already-split command segment. */
static int shell_run_segment(shell_t* sh, char* segment) {
    const char* argv[SHELL_MAX_ARGS];
    char expanded[TTY_BUFFER_SIZE * 2];
    expand_status(segment, expanded, sizeof(expanded), sh->last_status);
    int argc = parse_line(expanded, argv, SHELL_MAX_ARGS);
    if (!argc) {
        return sh->last_status;
    }
    return shell_exec(argv[0], argc, argv);
}

/* Execute a full line, honouring ;  &&  || outside quotes. */
static void shell_run_line(shell_t* sh, char* line) {
    char* start = line;
    char quote = 0;
    char op = ';';
    for (char* p = line;; p++) {
        char next_op = 0;
        size_t skip = 0;
        if (*p == '\0') {
            next_op = '\0';
        } else if (quote) {
            if (*p == '\\' && quote == '"' && p[1]) {
                p++;
            } else if (*p == quote) {
                quote = 0;
            }
            continue;
        } else if (*p == '\\' && p[1]) {
            p++;
            continue;
        } else if (*p == '\'' || *p == '"') {
            quote = *p;
            continue;
        } else if (*p == ';') {
            next_op = ';';
            skip = 1;
        } else if (p[0] == '&' && p[1] == '&') {
            next_op = '&';
            skip = 2;
        } else if (p[0] == '|' && p[1] == '|') {
            next_op = '|';
            skip = 2;
        } else {
            continue;
        }

        bool at_end = (*p == '\0');
        if (!at_end) {
            *p = '\0';
        }
        bool run = op == ';' || (op == '&' && sh->last_status == 0) ||
                   (op == '|' && sh->last_status != 0);
        if (run && sh->running) {
            sh->last_status = shell_run_segment(sh, start);
        }
        if (at_end || !sh->running) {
            break;
        }
        p += skip - 1;
        start = p + 1;
        op = next_op;
    }
}

void shell_run(shell_t* sh) {
    if (!sh || !sh->tty) {
        return;
    }
    g_active_shell = sh;
    sh->tty->prompt_fn = shell_print_prompt;
    sh->tty->complete_fn = shell_complete;
    shell_printf("tach (%s)\n", TACH_VERSION_STRING);
    shell_printf("Type 'help' for available commands.\n\n");

    char line[TTY_BUFFER_SIZE];
    while (sh->running) {
        shell_print_prompt();
        ssize_t length = tty_readline(sh->tty, line, sizeof(line));
        if (length <= 0) {
            continue;
        }
        shell_run_line(sh, line);
    }
    sh->tty->prompt_fn = NULL;
    sh->tty->complete_fn = NULL;
    g_active_shell = NULL;
}
