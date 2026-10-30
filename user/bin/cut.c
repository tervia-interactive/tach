#include <tach.h>
#include <tachc.h>

/* cut -f LISTA [-d DELIM] [-s] [arquivo...]   (campos)
 * cut -c LISTA [arquivo...]                    (caracteres/bytes)
 * LISTA: "1,3-5,7-" (posicoes comecam em 1). Linhas truncadas em 1023 bytes. */

#define LINE_CAP 1024
#define RANGE_MAX 16

struct range {
    unsigned long low, high;
};

static struct range g_ranges[RANGE_MAX];
static int g_range_count;
static char g_line[LINE_CAP];
static struct rbuf g_reader;
static struct wbuf g_writer;

static int parse_list(const char* text) {
    while (*text) {
        if (g_range_count == RANGE_MAX) return -1;
        struct range* range = &g_ranges[g_range_count];
        char* end;
        range->low = 1;
        range->high = (unsigned long)-1;
        if (*text != '-') {
            range->low = strtoul(text, &end, 10);
            if (end == text || range->low == 0) return -1;
            text = end;
            range->high = range->low;
        }
        if (*text == '-') {
            text++;
            if (isdigit((unsigned char)*text)) {
                range->high = strtoul(text, &end, 10);
                text = end;
            } else {
                range->high = (unsigned long)-1;
            }
        }
        if (range->high < range->low) return -1;
        g_range_count++;
        if (*text == ',') text++;
        else if (*text) return -1;
    }
    return g_range_count ? 0 : -1;
}

static int selected(unsigned long position) {
    for (int i = 0; i < g_range_count; i++)
        if (position >= g_ranges[i].low && position <= g_ranges[i].high) return 1;
    return 0;
}

static int cut_fd(int fd, int fields, char delimiter, int only_delimited) {
    rb_init(&g_reader, fd);
    for (;;) {
        long length = rb_getline(&g_reader, g_line, sizeof(g_line));
        if (length < 0) return 0;
        if (!fields) {
            for (long i = 0; i < length; i++)
                if (selected((unsigned long)i + 1)) wb_putc(&g_writer, g_line[i]);
            wb_putc(&g_writer, '\n');
            continue;
        }
        if (!strchr(g_line, delimiter)) {
            if (!only_delimited) {
                wb_write(&g_writer, g_line, (size_t)length);
                wb_putc(&g_writer, '\n');
            }
            continue;
        }
        unsigned long field = 1;
        int first = 1;
        const char* start = g_line;
        for (;;) {
            const char* stop = strchr(start, delimiter);
            size_t size = stop ? (size_t)(stop - start) : strlen(start);
            if (selected(field)) {
                if (!first) wb_putc(&g_writer, delimiter);
                wb_write(&g_writer, start, size);
                first = 0;
            }
            if (!stop) break;
            start = stop + 1;
            field++;
        }
        wb_putc(&g_writer, '\n');
    }
}

int main(int argc, char** argv) {
    const char* list = (const char*)0;
    int fields = 0;
    char delimiter = '\t';
    int only_delimited = 0;
    int c;
    while ((c = getopt(argc, argv, "f:c:b:d:s")) != -1) {
        if (c == 'f') { list = optarg; fields = 1; }
        else if (c == 'c' || c == 'b') { list = optarg; fields = 0; }
        else if (c == 'd') {
            if (strlen(optarg) != 1) {
                eprintf("cut: the delimiter must be a single character\n");
                return 2;
            }
            delimiter = optarg[0];
        } else if (c == 's') only_delimited = 1;
        else {
            eprintf("usage: cut -f LIST [-d D] [-s] | -c LIST [file...]\n");
            return 2;
        }
    }
    if (!list || parse_list(list) < 0) {
        eprintf("cut: missing or invalid list\n");
        return 2;
    }
    wb_init(&g_writer, 1);
    int status = 0;
    if (optind >= argc) {
        cut_fd(0, fields, delimiter, only_delimited);
    } else {
        for (int i = optind; i < argc; i++) {
            int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
            if (fd < 0) {
                eprintf("cut: %s: cannot open\n", argv[i]);
                status = 1;
                continue;
            }
            cut_fd(fd, fields, delimiter, only_delimited);
            if (fd != 0) (void)close(fd);
        }
    }
    if (wb_flush(&g_writer) < 0) status = 1;
    return status;
}
