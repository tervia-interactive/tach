#include <tach.h>
#include <tachc.h>

/* comm [-1] [-2] [-3] arq1 arq2: compara dois arquivos ORDENADOS linha a
 * linha. Coluna 1: so em arq1; coluna 2 (1 TAB): so em arq2; coluna 3
 * (2 TABs): em ambos. -N suprime a coluna N. "-" le a entrada padrao. */

#define LINE_CAP 1024

static struct rbuf g_r1, g_r2;
static struct wbuf g_writer;
static int g_hide[4];

static void emit(int column, const char* line) {
    if (g_hide[column]) return;
    int tabs = 0;
    for (int c = 1; c < column; c++)
        if (!g_hide[c]) tabs++;
    for (int i = 0; i < tabs; i++) wb_putc(&g_writer, '\t');
    wb_write(&g_writer, line, strlen(line));
    wb_putc(&g_writer, '\n');
}

static int open_input(const char* path) {
    if (strcmp(path, "-") == 0) return 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) eprintf("comm: %s: cannot open\n", path);
    return fd;
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "123")) != -1) {
        if (c >= '1' && c <= '3') g_hide[c - '0'] = 1;
        else {
            eprintf("usage: comm [-123] file1 file2\n");
            return 2;
        }
    }
    if (argc - optind != 2) {
        eprintf("usage: comm [-123] file1 file2\n");
        return 2;
    }
    int fd1 = open_input(argv[optind]);
    if (fd1 < 0) return 2;
    int fd2 = open_input(argv[optind + 1]);
    if (fd2 < 0) return 2;
    rb_init(&g_r1, fd1);
    rb_init(&g_r2, fd2);
    wb_init(&g_writer, 1);

    static char a[LINE_CAP], b[LINE_CAP];
    long la = rb_getline(&g_r1, a, sizeof(a));
    long lb = rb_getline(&g_r2, b, sizeof(b));
    while (la >= 0 || lb >= 0) {
        int order;
        if (la < 0) order = 1;
        else if (lb < 0) order = -1;
        else order = strcmp(a, b);
        if (order < 0) {
            emit(1, a);
            la = rb_getline(&g_r1, a, sizeof(a));
        } else if (order > 0) {
            emit(2, b);
            lb = rb_getline(&g_r2, b, sizeof(b));
        } else {
            emit(3, a);
            la = rb_getline(&g_r1, a, sizeof(a));
            lb = rb_getline(&g_r2, b, sizeof(b));
        }
    }
    int status = wb_flush(&g_writer) < 0 ? 1 : 0;
    if (fd1 > 0) (void)close(fd1);
    if (fd2 > 0) (void)close(fd2);
    return status;
}
