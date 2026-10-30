#include <tach.h>
#include <tachc.h>

/* wc [-lwcm] [arquivo...]: linhas, palavras e bytes. */

static unsigned char g_buf[2048];

struct counts {
    unsigned long lines, words, bytes;
};

static int count_fd(int fd, struct counts* totals) {
    int in_word = 0;
    for (;;) {
        ssize_t count = read(fd, g_buf, sizeof(g_buf));
        if (count < 0) return -1;
        if (count == 0) return 0;
        for (ssize_t i = 0; i < count; i++) {
            unsigned char c = g_buf[i];
            if (c == '\n') totals->lines++;
            if (isspace(c)) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                totals->words++;
            }
        }
        totals->bytes += (unsigned long)count;
    }
}

static void show(const struct counts* counts, int want_l, int want_w,
                 int want_c, const char* name) {
    if (want_l) printf("%7lu", counts->lines);
    if (want_w) printf("%7lu", counts->words);
    if (want_c) printf("%7lu", counts->bytes);
    if (name) printf(" %s", name);
    printf("\n");
}

int main(int argc, char** argv) {
    int want_l = 0, want_w = 0, want_c = 0;
    int c;
    while ((c = getopt(argc, argv, "lwcm")) != -1) {
        if (c == 'l') want_l = 1;
        else if (c == 'w') want_w = 1;
        else if (c == 'c' || c == 'm') want_c = 1;
        else {
            eprintf("usage: wc [-lwc] [file...]\n");
            return 2;
        }
    }
    if (!want_l && !want_w && !want_c) want_l = want_w = want_c = 1;

    int files = argc - optind;
    struct counts counts = {0, 0, 0};
    if (files <= 0) {
        if (count_fd(0, &counts) < 0) return 1;
        show(&counts, want_l, want_w, want_c, (const char*)0);
        return 0;
    }

    int status = 0;
    struct counts total = {0, 0, 0};
    for (int i = optind; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("wc: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        memset(&counts, 0, sizeof(counts));
        if (count_fd(fd, &counts) < 0) {
            eprintf("wc: %s: read error\n", argv[i]);
            status = 1;
        } else {
            show(&counts, want_l, want_w, want_c, argv[i]);
            total.lines += counts.lines;
            total.words += counts.words;
            total.bytes += counts.bytes;
        }
        if (fd != 0) (void)close(fd);
    }
    if (files > 1) show(&total, want_l, want_w, want_c, "total");
    return status;
}
