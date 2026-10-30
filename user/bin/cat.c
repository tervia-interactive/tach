#include <tach.h>
#include <tachc.h>

/* cat [-n] [arquivo...]: concatena arquivos (ou stdin). Existe no /bin para
 * pipelines, xargs e redirecao; o shell tem um "cat" interno para arquivos. */

static unsigned char g_buf[2048];
static struct rbuf g_reader;
static struct wbuf g_writer;

static int cat_fd(int fd, int number, unsigned long* line) {
    if (!number) {
        for (;;) {
            ssize_t count = read(fd, g_buf, sizeof(g_buf));
            if (count < 0) return -1;
            if (count == 0) return 0;
            if (write_all(1, g_buf, (size_t)count) < 0) return -1;
        }
    }
    rb_init(&g_reader, fd);
    int at_start = 1;
    int ch;
    while ((ch = rb_getc(&g_reader)) >= 0) {
        if (at_start) {
            char prefix[24];
            int n = snprintf(prefix, sizeof(prefix), "%6lu\t", ++*line);
            wb_write(&g_writer, prefix, (size_t)n);
            at_start = 0;
        }
        wb_putc(&g_writer, (char)ch);
        if (ch == '\n') at_start = 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    int number = 0;
    int c;
    while ((c = getopt(argc, argv, "n")) != -1) {
        if (c == 'n') number = 1;
        else {
            eprintf("usage: cat [-n] [file...]\n");
            return 2;
        }
    }
    wb_init(&g_writer, 1);
    unsigned long line = 0;
    int status = 0;
    if (optind >= argc) {
        if (cat_fd(0, number, &line) < 0) status = 1;
    }
    for (int i = optind; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("cat: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        if (cat_fd(fd, number, &line) < 0) {
            eprintf("cat: %s: read/write error\n", argv[i]);
            status = 1;
        }
        if (fd != 0) (void)close(fd);
    }
    if (wb_flush(&g_writer) < 0) status = 1;
    return status;
}
