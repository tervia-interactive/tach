#include <tach.h>
#include <tachc.h>

/* hexdump [-C] [-v] [-n BYTES] [-s SKIP] [arquivo]
 * Formato canonico (como "hexdump -C"): offset, 16 bytes em hex, ASCII.
 * Linhas repetidas viram "*" (use -v para mostrar todas). */

static unsigned char g_buf[1024];
static unsigned char g_row[16];
static unsigned char g_prev[16];

static void print_row(unsigned long offset, const unsigned char* row,
                      size_t length) {
    char line[96];
    size_t position = 0;
    position += (size_t)snprintf(line + position, sizeof(line) - position,
                                 "%08lx ", offset);
    for (size_t i = 0; i < 16; i++) {
        if (i == 8) line[position++] = ' ';
        if (i < length)
            position += (size_t)snprintf(line + position,
                                         sizeof(line) - position, " %02x", row[i]);
        else {
            line[position++] = ' ';
            line[position++] = ' ';
            line[position++] = ' ';
        }
    }
    line[position++] = ' ';
    line[position++] = ' ';
    line[position++] = '|';
    for (size_t i = 0; i < length; i++)
        line[position++] = isprint(row[i]) ? (char)row[i] : '.';
    line[position++] = '|';
    line[position++] = '\n';
    (void)write_all(1, line, position);
}

int main(int argc, char** argv) {
    unsigned long limit = (unsigned long)-1;
    unsigned long skip = 0;
    int verbose = 0;
    int c;
    while ((c = getopt(argc, argv, "Cvn:s:")) != -1) {
        char* end;
        if (c == 'C') continue;
        if (c == 'v') { verbose = 1; continue; }
        if (c == 'n' || c == 's') {
            long value = strtol(optarg, &end, 0);
            if (*end || value < 0) {
                eprintf("hexdump: invalid number: %s\n", optarg);
                return 2;
            }
            if (c == 'n') limit = (unsigned long)value;
            else skip = (unsigned long)value;
            continue;
        }
        eprintf("usage: hexdump [-C] [-v] [-n bytes] [-s skip] [file]\n");
        return 2;
    }
    int fd = 0;
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        fd = open(argv[optind], O_RDONLY);
        if (fd < 0) {
            eprintf("hexdump: %s: cannot open\n", argv[optind]);
            return 1;
        }
    }

    unsigned long offset = 0;   /* offset absoluto da proxima linha */
    unsigned long consumed = 0; /* bytes ja descartados pelo -s */
    size_t filled = 0;
    int have_prev = 0, starred = 0;
    unsigned long emitted = 0;

    for (;;) {
        ssize_t count = read(fd, g_buf, sizeof(g_buf));
        if (count < 0) {
            eprintf("hexdump: read error\n");
            return 1;
        }
        if (count == 0) break;
        for (ssize_t i = 0; i < count; i++) {
            if (consumed < skip) { consumed++; offset++; continue; }
            if (emitted >= limit) goto finish;
            g_row[filled++] = g_buf[i];
            emitted++;
            if (filled == 16) {
                if (!verbose && have_prev && memcmp(g_row, g_prev, 16) == 0) {
                    if (!starred) { (void)write_all(1, "*\n", 2); starred = 1; }
                } else {
                    print_row(offset, g_row, 16);
                    starred = 0;
                }
                memcpy(g_prev, g_row, 16);
                have_prev = 1;
                offset += 16;
                filled = 0;
            }
        }
    }
finish:
    if (filled) {
        print_row(offset, g_row, filled);
        offset += filled;
    }
    printf("%08lx\n", offset);
    if (fd != 0) (void)close(fd);
    return 0;
}
