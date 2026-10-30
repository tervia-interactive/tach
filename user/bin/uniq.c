#include <tach.h>
#include <tachc.h>

/* uniq [-c] [-d] [-u] [-i] [arquivo]: colapsa linhas adjacentes iguais.
 * Linhas com mais de 1023 bytes sao truncadas. */

#define LINE_CAP 1024

static char g_prev[LINE_CAP];
static char g_cur[LINE_CAP];
static struct rbuf g_reader;
static struct wbuf g_writer;

static int g_count_mode, g_only_dups, g_only_uniq, g_icase;

static void flush_group(const char* line, unsigned long count) {
    if (g_only_dups && count < 2) return;
    if (g_only_uniq && count != 1) return;
    char prefix[24];
    if (g_count_mode) {
        int n = snprintf(prefix, sizeof(prefix), "%7lu ", count);
        wb_write(&g_writer, prefix, (size_t)n);
    }
    wb_write(&g_writer, line, strlen(line));
    wb_putc(&g_writer, '\n');
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "cdui")) != -1) {
        if (c == 'c') g_count_mode = 1;
        else if (c == 'd') g_only_dups = 1;
        else if (c == 'u') g_only_uniq = 1;
        else if (c == 'i') g_icase = 1;
        else {
            eprintf("usage: uniq [-cdui] [file]\n");
            return 2;
        }
    }
    int fd = 0;
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        fd = open(argv[optind], O_RDONLY);
        if (fd < 0) {
            eprintf("uniq: %s: cannot open\n", argv[optind]);
            return 1;
        }
    }
    rb_init(&g_reader, fd);
    wb_init(&g_writer, 1);

    unsigned long count = 0;
    while (rb_getline(&g_reader, g_cur, sizeof(g_cur)) >= 0) {
        int same = count > 0 &&
                   (g_icase ? strcasecmp(g_prev, g_cur) == 0
                            : strcmp(g_prev, g_cur) == 0);
        if (same) {
            count++;
            continue;
        }
        if (count) flush_group(g_prev, count);
        memcpy(g_prev, g_cur, strlen(g_cur) + 1);
        count = 1;
    }
    if (count) flush_group(g_prev, count);
    int status = wb_flush(&g_writer) < 0 ? 1 : 0;
    if (fd != 0) (void)close(fd);
    return status;
}
