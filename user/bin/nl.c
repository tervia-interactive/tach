#include <tach.h>
#include <tachc.h>

/* nl [-b a|t] [arquivo]: numera linhas (-b a todas, -b t so nao vazias). */

static struct rbuf g_reader;
static struct wbuf g_writer;

static void write_number(unsigned long number) {
    char text[24];
    int length = snprintf(text, sizeof(text), "%6lu\t", number);
    wb_write(&g_writer, text, (size_t)length);
}

int main(int argc, char** argv) {
    int number_all = 0;
    int c;
    while ((c = getopt(argc, argv, "b:")) != -1) {
        if (c == 'b' && (strcmp(optarg, "a") == 0 || strcmp(optarg, "t") == 0)) {
            number_all = (optarg[0] == 'a');
        } else {
            eprintf("usage: nl [-b a|t] [file]\n");
            return 2;
        }
    }
    int fd = 0;
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        fd = open(argv[optind], O_RDONLY);
        if (fd < 0) {
            eprintf("nl: %s: cannot open\n", argv[optind]);
            return 1;
        }
    }
    rb_init(&g_reader, fd);
    wb_init(&g_writer, 1);

    unsigned long number = 0;
    int at_start = 1;
    int c2;
    while ((c2 = rb_getc(&g_reader)) >= 0) {
        if (at_start) {
            if (number_all || c2 != '\n') write_number(++number);
            else wb_write(&g_writer, "       \t", 8);
            at_start = 0;
        }
        wb_putc(&g_writer, (char)c2);
        if (c2 == '\n') at_start = 1;
    }
    int status = wb_flush(&g_writer) < 0 ? 1 : 0;
    if (fd != 0) (void)close(fd);
    return status;
}
