#include <tach.h>
#include <tachc.h>

/* rev [arquivo...]: inverte os caracteres de cada linha (bytes; linhas com
 * mais de 1023 bytes sao truncadas). */

#define LINE_CAP 1024

static char g_line[LINE_CAP];
static struct rbuf g_reader;
static struct wbuf g_writer;

static int rev_fd(int fd) {
    rb_init(&g_reader, fd);
    for (;;) {
        long length = rb_getline(&g_reader, g_line, sizeof(g_line));
        if (length < 0) break;
        for (long i = length; i > 0; i--) wb_putc(&g_writer, g_line[i - 1]);
        wb_putc(&g_writer, '\n');
    }
    return 0;
}

int main(int argc, char** argv) {
    wb_init(&g_writer, 1);
    int status = 0;
    if (argc < 2) {
        rev_fd(0);
    } else {
        for (int i = 1; i < argc; i++) {
            int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
            if (fd < 0) {
                eprintf("rev: %s: cannot open\n", argv[i]);
                status = 1;
                continue;
            }
            rev_fd(fd);
            if (fd != 0) (void)close(fd);
        }
    }
    if (wb_flush(&g_writer) < 0) status = 1;
    return status;
}
