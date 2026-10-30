#include <tach.h>
#include <tachc.h>

/* xargs [-n N] [comando [args...]]: le palavras da entrada padrao (separadas
 * por espaco/linha; aspas simples ou duplas agrupam) e as acrescenta ao
 * comando (padrao: echo), em lotes de N. O comando e procurado em /bin e
 * nao pode ser um builtin do shell. */

#define ARG_MAX_COUNT 32
#define TOKEN_CAP 2048

static char g_tokens[TOKEN_CAP];
static char* g_argv[ARG_MAX_COUNT + 1];
static struct rbuf g_reader;

static int run(const char* path, char** argv) {
    pid_t child = fork();
    if (child < 0) {
        eprintf("xargs: fork failed\n");
        return 1;
    }
    if (child == 0) {
        (void)execve(path, argv, (char* const*)0);
        eprintf("xargs: %s: cannot execute\n", path);
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) return 1;
    return status ? 1 : 0;
}

int main(int argc, char** argv) {
    long batch = 0; /* 0 = todos que couberem */
    int c;
    while ((c = getopt(argc, argv, "n:")) != -1) {
        if (c == 'n') {
            batch = strtol(optarg, (char**)0, 10);
            if (batch < 1) {
                eprintf("xargs: -n must be >= 1\n");
                return 2;
            }
        } else {
            eprintf("usage: xargs [-n N] [command [args...]]\n");
            return 2;
        }
    }
    static char path[128];
    const char* command = optind < argc ? argv[optind] : "echo";
    if (command[0] == '/') {
        if (strlcpy(path, command, sizeof(path)) >= sizeof(path)) return 2;
    } else {
        if (strlen(command) + 5 >= sizeof(path)) return 2;
        strcpy(path, "/bin/");
        strcpy(path + 5, command);
    }
    int fixed = 0;
    g_argv[fixed++] = (char*)command;
    for (int i = optind + 1; i < argc && fixed < ARG_MAX_COUNT; i++)
        g_argv[fixed++] = argv[i];

    int limit = ARG_MAX_COUNT - fixed;
    if (limit < 1) {
        eprintf("xargs: too many fixed arguments\n");
        return 2;
    }
    if (batch == 0 || batch > limit) batch = limit;

    rb_init(&g_reader, 0);
    size_t used = 0;
    int extra = 0;
    int status = 0;
    int ch = rb_getc(&g_reader);
    for (;;) {
        while (ch >= 0 && isspace(ch)) ch = rb_getc(&g_reader);
        if (ch < 0) break;
        int quote = (ch == '\'' || ch == '"') ? ch : 0;
        if (quote) ch = rb_getc(&g_reader);
        size_t start = used;
        while (ch >= 0 && (quote ? ch != quote : !isspace(ch))) {
            if (used + 2 < TOKEN_CAP) g_tokens[used++] = (char)ch;
            ch = rb_getc(&g_reader);
        }
        if (quote && ch == quote) ch = rb_getc(&g_reader);
        g_tokens[used++] = '\0';
        g_argv[fixed + extra++] = g_tokens + start;
        if (extra == batch || used + 64 >= TOKEN_CAP) {
            g_argv[fixed + extra] = (char*)0;
            if (run(path, g_argv)) status = 1;
            extra = 0;
            used = 0;
        }
    }
    if (extra > 0) {
        g_argv[fixed + extra] = (char*)0;
        if (run(path, g_argv)) status = 1;
    }
    return status;
}
