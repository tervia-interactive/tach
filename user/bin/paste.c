#include <tach.h>
#include <tachc.h>

/* paste [-d lista] arq1 arq2...: junta linhas correspondentes dos arquivos,
 * separadas por TAB (ou pelos caracteres de -d, usados ciclicamente).
 * "-" le a entrada padrao. Maximo de 8 arquivos. */

#define MAX_IN 8
#define LINE_CAP 1024

static struct rbuf g_readers[MAX_IN];
static struct wbuf g_writer;

int main(int argc, char** argv) {
    const char* delims = "\t";
    int c;
    while ((c = getopt(argc, argv, "d:")) != -1) {
        if (c == 'd') delims = optarg;
        else {
            eprintf("usage: paste [-d list] file...\n");
            return 2;
        }
    }
    size_t delim_count = strlen(delims);
    if (delim_count == 0) { delims = "\t"; delim_count = 1; }
    int count = argc - optind;
    if (count < 1) {
        eprintf("usage: paste [-d list] file...\n");
        return 2;
    }
    if (count > MAX_IN) {
        eprintf("paste: too many files (max %d)\n", MAX_IN);
        return 2;
    }
    int fds[MAX_IN];
    for (int i = 0; i < count; i++) {
        const char* path = argv[optind + i];
        fds[i] = strcmp(path, "-") == 0 ? 0 : open(path, O_RDONLY);
        if (fds[i] < 0) {
            eprintf("paste: %s: cannot open\n", path);
            return 1;
        }
        rb_init(&g_readers[i], fds[i]);
    }
    wb_init(&g_writer, 1);
    static char line[LINE_CAP];
    for (;;) {
        int any = 0;
        for (int i = 0; i < count; i++) {
            long length = rb_getline(&g_readers[i], line, sizeof(line));
            if (length >= 0) {
                any = 1;
                wb_write(&g_writer, line, (size_t)length);
            }
            if (i + 1 < count) wb_putc(&g_writer, delims[(size_t)i % delim_count]);
        }
        if (!any) break;
        wb_putc(&g_writer, '\n');
    }
    int status = wb_flush(&g_writer) < 0 ? 1 : 0;
    for (int i = 0; i < count; i++)
        if (fds[i] > 0) (void)close(fds[i]);
    return status;
}
