#include <tach.h>

/*
 * tach userspace shell.
 *
 * Porta, para o ring 3, os comandos do shell embutido no kernel
 * (src/term/shell.c): help clear echo uname ps id whoami pwd cd ls cat
 * history syscalls status true false exit, mais edicao de linha
 * (Backspace, Ctrl+U, Ctrl+W, historico com Cima/Baixo) e quoting.
 * Dados do kernel (ps, uname, meminfo, syscalls...) vem de /proc.
 *
 * A pilha de usuario tem uma unica pagina: buffers grandes ficam em
 * variaveis estaticas, nunca na pilha.
 */

#define LINE_LEN 256
#define ARGV_MAX 16
#define HIST_MAX 8 /* igual ao TTY_HISTORY_SIZE do shell antigo */
#define PATH_LEN 128
#define FILE_LEN 2048

static char g_history[HIST_MAX][LINE_LEN];
static unsigned g_hist_count;
static unsigned g_hist_head;
static char g_cwd[PATH_LEN];
static int g_last_status;
static char g_file[FILE_LEN + 1];

/* ------------------------------------------------------------ saida */

static void out(const char* text) { puts_fd(1, text); }
static void err(const char* text) { puts_fd(2, text); }

static void out_ulong(unsigned long value, unsigned width) {
    char digits[24];
    unsigned count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (width > count) {
        (void)write(1, " ", 1);
        width--;
    }
    while (count) (void)write(1, &digits[--count], 1);
}

static void out_long(long value) {
    if (value < 0) {
        out("-");
        out_ulong(0UL - (unsigned long)value, 0);
    } else {
        out_ulong((unsigned long)value, 0);
    }
}

/* ---------------------------------------------------------- strings */

static int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static int parse_small_int(const char* text, int fallback) {
    if (!text || !*text) return fallback;
    int value = 0;
    int negative = 0;
    if (*text == '-') {
        negative = 1;
        text++;
    }
    while (*text >= '0' && *text <= '9') value = value * 10 + (*text++ - '0');
    return negative ? -value : value;
}

/* ---------------------------------------------------------- arquivos */

/* Le o arquivo inteiro (ate FILE_LEN bytes) em g_file. -1 se nao abre. */
static int slurp(const char* path) {
    int fd = open(path, 0);
    if (fd < 0) return -1;
    size_t total = 0;
    while (total < FILE_LEN) {
        ssize_t n = read(fd, g_file + total, FILE_LEN - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    (void)close(fd);
    g_file[total] = 0;
    return (int)total;
}

/* Copia o arquivo para stdout. -1 se nao abre. */
static int cat_file(const char* path) {
    int fd = open(path, 0);
    if (fd < 0) return -1;
    for (;;) {
        ssize_t n = read(fd, g_file, FILE_LEN);
        if (n <= 0) break;
        (void)write(1, g_file, (size_t)n);
    }
    (void)close(fd);
    return 0;
}

/* Valor numerico de uma linha "Chave:\tN" em texto estilo /proc. */
static unsigned long field_ulong(const char* text, const char* key) {
    size_t key_len = strlen(key);
    while (*text) {
        if (starts_with(text, key)) {
            const char* p = text + key_len;
            unsigned long value = 0;
            while (*p == ' ' || *p == '\t') p++;
            while (*p >= '0' && *p <= '9')
                value = value * 10 + (unsigned long)(*p++ - '0');
            return value;
        }
        while (*text && *text != '\n') text++;
        if (*text) text++;
    }
    return 0;
}

/* ---------------------------------------------------------- historico */

static const char* hist_get(unsigned index) {
    unsigned oldest = g_hist_count < HIST_MAX ? 0 : g_hist_head;
    return g_history[(oldest + index) % HIST_MAX];
}

static void hist_add(const char* line) {
    if (!line[0]) return;
    if (g_hist_count) {
        const char* last = g_history[(g_hist_head + HIST_MAX - 1) % HIST_MAX];
        if (strcmp(last, line) == 0) return;
    }
    size_t i = 0;
    while (line[i] && i + 1 < LINE_LEN) {
        g_history[g_hist_head][i] = line[i];
        i++;
    }
    g_history[g_hist_head][i] = 0;
    g_hist_head = (g_hist_head + 1) % HIST_MAX;
    if (g_hist_count < HIST_MAX) g_hist_count++;
}

/* ------------------------------------------------------ edicao de linha */

static void erase_one(size_t* length) {
    out("\b \b");
    (*length)--;
}

static void replace_line(char* line, size_t* length, const char* text) {
    while (*length) erase_one(length);
    size_t n = 0;
    while (text[n] && n + 1 < LINE_LEN) {
        line[n] = text[n];
        n++;
    }
    line[n] = 0;
    *length = n;
    if (n) (void)write(1, line, n);
}

static void history_move(char* line, size_t* length, unsigned* cursor,
                         int direction) {
    if (!g_hist_count) return;
    if (direction < 0) {
        if (*cursor > 0) (*cursor)--;
    } else if (*cursor < g_hist_count) {
        (*cursor)++;
    }
    replace_line(line, length, *cursor == g_hist_count ? "" : hist_get(*cursor));
}

static size_t read_line(char* line, size_t capacity) {
    size_t length = 0;
    unsigned cursor = g_hist_count;
    line[0] = 0;
    for (;;) {
        char c;
        if (read(0, &c, 1) <= 0) continue;
        if (c == '\r' || c == '\n') {
            out("\n");
            break;
        }
        if (c == 27) { /* ESC [ A / ESC [ B vindos da serial */
            char bracket, key;
            if (read(0, &bracket, 1) <= 0 || read(0, &key, 1) <= 0) continue;
            if (bracket == '[' && key == 'A')
                history_move(line, &length, &cursor, -1);
            else if (bracket == '[' && key == 'B')
                history_move(line, &length, &cursor, 1);
            continue;
        }
        if (c == 0x10) { /* PS/2 cima */
            history_move(line, &length, &cursor, -1);
            continue;
        }
        if (c == 0x0e) { /* PS/2 baixo */
            history_move(line, &length, &cursor, 1);
            continue;
        }
        if (c == 0x15) { /* Ctrl+U */
            while (length) erase_one(&length);
            line[0] = 0;
            continue;
        }
        if (c == 0x17) { /* Ctrl+W */
            while (length && line[length - 1] == ' ') erase_one(&length);
            while (length && line[length - 1] != ' ') erase_one(&length);
            line[length] = 0;
            continue;
        }
        if (c == '\b' || c == 127) {
            if (length) erase_one(&length);
            line[length] = 0;
            continue;
        }
        if (c == '\t') c = ' ';
        if (c < ' ' || c > '~') continue; /* ignora NUL/0xFF/controle */
        if (length + 1 < capacity) {
            line[length++] = c;
            line[length] = 0;
            (void)write(1, &c, 1);
        }
    }
    line[length] = 0;
    return length;
}

/* Quoting ('...' e "..."), escape com barra invertida, espaco e tab. */
static int parse_line(char* line, char** argv, int capacity) {
    int argc = 0;
    char* cursor = line;
    while (*cursor && argc + 1 < capacity) {
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (!*cursor) break;

        char quote = 0;
        if (*cursor == '\'' || *cursor == '"') quote = *cursor++;
        argv[argc++] = cursor;

        char* dest = cursor;
        while (*cursor) {
            if (quote) {
                if (*cursor == quote) {
                    cursor++;
                    break;
                }
            } else if (*cursor == ' ' || *cursor == '\t') {
                break;
            }
            if (*cursor == '\\' && cursor[1]) cursor++;
            *dest++ = *cursor++;
        }
        int had_separator = !quote && *cursor != '\0';
        *dest = '\0';
        if (had_separator) cursor++;
        while (*cursor == ' ' || *cursor == '\t') *cursor++ = '\0';
    }
    argv[argc] = (char*)0;
    return argc;
}

/* ----------------------------------------------------------- comandos */

static int is_dir(const char* path) {
    return strcmp(path, "/") == 0 || strcmp(path, "/dev") == 0 ||
           strcmp(path, "/proc") == 0 || strcmp(path, "/sbin") == 0 ||
           strcmp(path, "/bin") == 0;
}

static void command_help(void) {
    out("Built-in commands:\n"
        "  help                 show this command list\n"
        "  clear                clear the display\n"
        "  echo [args...]       print arguments\n"
        "  uname [-a]           show system information\n"
        "  ps                   list processes\n"
        "  id | whoami          show current identity\n"
        "  pid                  show this shell's pid\n"
        "  pwd | cd [path]      inspect or change logical directory\n"
        "  ls [path]            list the root filesystem\n"
        "  cat <file>...        print files (/proc/version, /proc/meminfo, ...)\n"
        "  history              show command history\n"
        "  syscalls             show active syscall ABI entries\n"
        "  status               show the previous command status\n"
        "  true | false         succeed / fail\n"
        "  exit [status]        leave the shell\n"
        "  /path | command      run an ELF from the initrd (/bin searched)\n"
        "\n"
        "Editing: Backspace, Ctrl+U, Ctrl+W, Up/Down history.\n");
}

static void command_id(void) {
    unsigned long uid = 0, gid = 0, pid = (unsigned long)getpid();
    if (slurp("/proc/self/status") > 0) {
        uid = field_ulong(g_file, "Uid:");
        gid = field_ulong(g_file, "Gid:");
        pid = field_ulong(g_file, "Pid:");
    }
    out("uid="); out_ulong(uid, 0);
    out("(root) gid="); out_ulong(gid, 0);
    out("(root) pid="); out_ulong(pid, 0);
    out("\n");
}

static void command_history(void) {
    if (!g_hist_count) {
        out("No history yet.\n");
        return;
    }
    for (unsigned i = 0; i < g_hist_count; i++) {
        out_ulong(i + 1, 2);
        out("  ");
        out(hist_get(i));
        out("\n");
    }
}

static int command_ls(const char* path) {
    if (strcmp(path, "/") == 0) {
        out("bin/\ndev/\nproc/\nsbin/\n");
    } else if (strcmp(path, "/bin") == 0) {
        out("sh\n");
    } else if (strcmp(path, "/dev") == 0) {
        out("console\ntty\n");
    } else if (strcmp(path, "/sbin") == 0) {
        out("init\n");
    } else if (strcmp(path, "/proc") == 0) {
        out("version\nmeminfo\nuname\nps\nsyscalls\nself/\n");
        if (slurp("/proc/ps") > 0) { /* um diretorio por PID vivo */
            const char* line = g_file;
            int header = 1;
            while (*line) {
                if (!header && *line != '\n') {
                    const char* p = line + 1; /* pula o marcador '*' */
                    unsigned long pid = 0;
                    int digits = 0;
                    while (*p == ' ') p++;
                    while (*p >= '0' && *p <= '9') {
                        pid = pid * 10 + (unsigned long)(*p++ - '0');
                        digits = 1;
                    }
                    if (digits) {
                        out_ulong(pid, 0);
                        out("/\n");
                    }
                }
                header = 0;
                while (*line && *line != '\n') line++;
                if (*line) line++;
            }
        }
    } else {
        err("ls: "); err(path); err(": no such directory\n");
        return 1;
    }
    return 0;
}

static int command_cat(int argc, char** argv) {
    if (argc < 2) {
        err("cat: missing operand\n");
        return 1;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (cat_file(argv[i]) < 0) {
            err("cat: "); err(argv[i]); err(": no such file\n");
            status = 1;
        }
    }
    return status;
}

static int command_cd(const char* path) {
    if (!is_dir(path)) {
        err("cd: "); err(path); err(": no such directory\n");
        return 1;
    }
    size_t i = 0;
    while (path[i] && i + 1 < sizeof(g_cwd)) {
        g_cwd[i] = path[i];
        i++;
    }
    g_cwd[i] = 0;
    return 0;
}

static int run_external(char** argv) {
    char path[PATH_LEN];
    const char* cmd = argv[0];
    size_t length = strlen(cmd);
    if (cmd[0] == '/') {
        if (length >= sizeof(path)) {
            err(cmd); err(": name too long\n");
            return 1;
        }
        strcpy(path, cmd);
    } else {
        if (length + 5 >= sizeof(path)) {
            err(cmd); err(": name too long\n");
            return 1;
        }
        strcpy(path, "/bin/");
        strcpy(path + 5, cmd);
    }
    pid_t child = fork();
    if (child < 0) {
        err("sh: fork failed\n");
        return 1;
    }
    if (child == 0) {
        (void)execve(path, argv, (char* const*)0);
        err(cmd); err(": command not found or not executable\n");
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        err("sh: wait failed\n");
        return 1;
    }
    return status;
}

static int run_command(int argc, char** argv) {
    const char* cmd = argv[0];
    if (strcmp(cmd, "help") == 0) {
        command_help();
    } else if (strcmp(cmd, "clear") == 0) {
        out("\f"); /* o console VGA trata FF como "limpar tela" */
    } else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) out(" ");
            out(argv[i]);
        }
        out("\n");
    } else if (strcmp(cmd, "uname") == 0) {
        if (argc > 1 && strcmp(argv[1], "-a") == 0) {
            if (cat_file("/proc/uname") < 0) out("tach\n");
        } else {
            out("tach\n");
        }
    } else if (strcmp(cmd, "ps") == 0) {
        if (cat_file("/proc/ps") < 0) {
            err("ps: /proc/ps unavailable\n");
            return 1;
        }
    } else if (strcmp(cmd, "id") == 0) {
        command_id();
    } else if (strcmp(cmd, "whoami") == 0) {
        out("root\n");
    } else if (strcmp(cmd, "pid") == 0) {
        out("pid="); out_long((long)getpid()); out("\n");
    } else if (strcmp(cmd, "pwd") == 0) {
        out(g_cwd); out("\n");
    } else if (strcmp(cmd, "cd") == 0) {
        return command_cd(argc > 1 ? argv[1] : "/");
    } else if (strcmp(cmd, "ls") == 0) {
        return command_ls(argc > 1 ? argv[1] : g_cwd);
    } else if (strcmp(cmd, "cat") == 0) {
        return command_cat(argc, argv);
    } else if (strcmp(cmd, "history") == 0) {
        command_history();
    } else if (strcmp(cmd, "syscalls") == 0) {
        if (cat_file("/proc/syscalls") < 0) {
            err("syscalls: /proc/syscalls unavailable\n");
            return 1;
        }
    } else if (strcmp(cmd, "status") == 0) {
        out_long((long)g_last_status); out("\n");
    } else if (strcmp(cmd, "true") == 0) {
        return 0;
    } else if (strcmp(cmd, "false") == 0) {
        return 1;
    } else if (strcmp(cmd, "exit") == 0) {
        _exit(argc > 1 ? parse_small_int(argv[1], 0) : 0);
    } else {
        return run_external(argv);
    }
    return 0;
}

int main(void) {
    g_hist_count = 0;
    g_hist_head = 0;
    g_last_status = 0;
    g_cwd[0] = '/';
    g_cwd[1] = 0;

    out("tach userspace shell\nType 'help' for commands.\n");
    char line[LINE_LEN];
    char* argv[ARGV_MAX];
    for (;;) {
        out("tach:"); out(g_cwd); out("# ");
        if (read_line(line, sizeof(line)) == 0) continue;
        hist_add(line);
        int argc = parse_line(line, argv, ARGV_MAX);
        if (!argc) continue;
        g_last_status = run_command(argc, argv);
    }
}
