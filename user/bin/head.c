#include <tach.h>
#include <tachc.h>

/* head [-n N] [-c N] [arquivo...]: primeiras N linhas (padrao 10) ou bytes. */

static unsigned char g_buf[2048];

static int head_fd(int fd, unsigned long limit, int by_bytes) {
    unsigned long remaining = limit;
    while (remaining) {
        ssize_t count = read(fd, g_buf, sizeof(g_buf));
        if (count < 0) return 1;
        if (count == 0) break;
        size_t take = 0;
        if (by_bytes) {
            take = remaining < (unsigned long)count ? (size_t)remaining
                                                    : (size_t)count;
            remaining -= take;
        } else {
            while (take < (size_t)count && remaining)
                if (g_buf[take++] == '\n') remaining--;
        }
        if (write_all(1, g_buf, take) < 0) return 1;
    }
    return 0;
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
                eprintf("head: invalid count: %s\n", optarg);
                return 2;
            }
            limit = (unsigned long)value;
            by_bytes = (c == 'c');
        } else {
            eprintf("usage: head [-n N] [-c N] [file...]\n");
            return 2;
        }
    }
    int files = argc - optind;
    if (files <= 0) return head_fd(0, limit, by_bytes);

    int status = 0;
    for (int i = optind; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("head: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        if (files > 1) printf("%s==> %s <==\n", i > optind ? "\n" : "", argv[i]);
        if (head_fd(fd, limit, by_bytes)) status = 1;
        if (fd != 0) (void)close(fd);
    }
    return status;
}
