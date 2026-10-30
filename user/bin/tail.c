#include <tach.h>
#include <tachc.h>

/* tail [-n N] [-c N] [arquivo...]: ultimas N linhas (padrao 10) ou bytes.
 * Mantem so os ultimos TAIL_CAP bytes lidos (limite do utilitario). */

#define TAIL_CAP 16384

static unsigned char g_tail[TAIL_CAP];
static unsigned char g_chunk[2048];

static int tail_fd(int fd, unsigned long limit, int by_bytes) {
    size_t length = 0;
    for (;;) {
        ssize_t count = read(fd, g_chunk, sizeof(g_chunk));
        if (count < 0) return 1;
        if (count == 0) break;
        size_t n = (size_t)count;
        if (n >= TAIL_CAP) {
            memcpy(g_tail, g_chunk + (n - TAIL_CAP), TAIL_CAP);
            length = TAIL_CAP;
            continue;
        }
        if (length + n > TAIL_CAP) {
            size_t drop = length + n - TAIL_CAP;
            memmove(g_tail, g_tail + drop, length - drop);
            length -= drop;
        }
        memcpy(g_tail + length, g_chunk, n);
        length += n;
    }
    if (!limit || !length) return 0;

    size_t start = 0;
    if (by_bytes) {
        start = (unsigned long)length > limit ? length - (size_t)limit : 0;
    } else {
        size_t i = length;
        if (g_tail[i - 1] == '\n') i--;
        unsigned long seen = 0;
        while (i > 0) {
            if (g_tail[i - 1] == '\n' && ++seen == limit) {
                start = i;
                break;
            }
            i--;
        }
    }
    return write_all(1, g_tail + start, length - start) < 0 ? 1 : 0;
}

int main(int argc, char** argv) {
    unsigned long limit = 10;
    int by_bytes = 0;
    int c;
    while ((c = getopt(argc, argv, "n:c:")) != -1) {
        if (c == 'n' || c == 'c') {
            char* end;
            long value = strtol(optarg, &end, 10);
            if (*end || value < 0) {
                eprintf("tail: invalid count: %s\n", optarg);
                return 2;
            }
            limit = (unsigned long)value;
            by_bytes = (c == 'c');
        } else {
            eprintf("usage: tail [-n N] [-c N] [file...]\n");
            return 2;
        }
    }
    int files = argc - optind;
    if (files <= 0) return tail_fd(0, limit, by_bytes);

    int status = 0;
    for (int i = optind; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("tail: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        if (files > 1) printf("%s==> %s <==\n", i > optind ? "\n" : "", argv[i]);
        if (tail_fd(fd, limit, by_bytes)) status = 1;
        if (fd != 0) (void)close(fd);
    }
    return status;
}
