#include <tach.h>
#include <tachc.h>

/* tee [-a] [arquivo...]: copia stdin para stdout e para cada arquivo. */

#define MAX_OUT 8

static unsigned char g_buf[2048];

int main(int argc, char** argv) {
    int append = 0;
    int c;
    while ((c = getopt(argc, argv, "a")) != -1) {
        if (c == 'a') append = 1;
        else {
            eprintf("usage: tee [-a] [file...]\n");
            return 2;
        }
    }
    int fds[MAX_OUT];
    int count = 0;
    int status = 0;
    for (int i = optind; i < argc; i++) {
        if (count == MAX_OUT) {
            eprintf("tee: too many files (max %d)\n", MAX_OUT);
            return 2;
        }
        int fd = open(argv[i], O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC));
        if (fd < 0) {
            eprintf("tee: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        fds[count++] = fd;
    }
    for (;;) {
        ssize_t n = read(0, g_buf, sizeof(g_buf));
        if (n < 0) { status = 1; break; }
        if (n == 0) break;
        if (write_all(1, g_buf, (size_t)n) < 0) status = 1;
        for (int i = 0; i < count; i++)
            if (write_all(fds[i], g_buf, (size_t)n) < 0) status = 1;
    }
    for (int i = 0; i < count; i++) (void)close(fds[i]);
    return status;
}
