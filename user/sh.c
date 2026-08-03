#include <tach.h>

static int read_line(char* line, size_t capacity) {
    size_t length = 0;
    while (length + 1 < capacity) {
        char c;
        if (read(0, &c, 1) <= 0) continue;
        if (c == '\r' || c == '\n') {
            puts_fd(1, "\n");
            break;
        }
        if ((c == '\b' || c == 127) && length) {
            length--; puts_fd(1, "\b \b"); continue;
        }
        line[length++] = c;
        (void)write(1, &c, 1);
    }
    line[length] = 0;
    return (int)length;
}

static int split(char* line, char** argv, int capacity) {
    int argc = 0;
    while (*line && argc + 1 < capacity) {
        while (*line == ' ') line++;
        if (!*line) break;
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
        if (*line) *line++ = 0;
    }
    argv[argc] = (char*)0;
    return argc;
}

int main(void) {
    puts_fd(1, "tach userspace shell\nType 'help' for commands.\n");
    char line[256];
    char* argv[16];
    for (;;) {
        puts_fd(1, "tach:/# ");
        if (!read_line(line, sizeof(line))) continue;
        int argc = split(line, argv, 16);
        if (!argc) continue;
        if (strcmp(argv[0], "help") == 0) {
            puts_fd(1, "builtins: help echo pid clear exit\n");
            puts_fd(1, "programs: ls cat mkdir rm touch write cp sync uname\n");
        } else if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                if (i > 1) puts_fd(1, " ");
                puts_fd(1, argv[i]);
            }
            puts_fd(1, "\n");
        } else if (strcmp(argv[0], "pid") == 0) {
            pid_t value = getpid();
            while (value >= 10) value -= 10;
            char digit = (char)('0' + value);
            puts_fd(1, "pid="); (void)write(1, &digit, 1); puts_fd(1, "\n");
        } else if (strcmp(argv[0], "clear") == 0) {
            puts_fd(1, "\033[2J\033[H");
        } else if (strcmp(argv[0], "exit") == 0) {
            return 0;
        } else {
            char path[128];
            if (argv[0][0] == '/') strcpy(path, argv[0]);
            else { strcpy(path, "/bin/"); strcpy(path + 5, argv[0]); }
            pid_t child = fork();
            if (child == 0) { execve(path, argv, (char* const*)0); _exit(127); }
            int status = 0;
            if (child < 0 || waitpid(child, &status, 0) < 0)
                puts_fd(2, "sh: execute failed\n");
        }
    }
}
