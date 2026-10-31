#include <tach.h>
#include <tachc.h>

/* cmp [-s] arq1 arq2: compara dois arquivos byte a byte.
 * Saida 0 = iguais, 1 = diferem, 2 = erro. Com -s nao imprime nada. */

int main(int argc, char** argv) {
    int silent = 0;
    int c;
    while ((c = getopt(argc, argv, "s")) != -1) {
        if (c == 's') silent = 1;
        else {
            eprintf("usage: cmp [-s] file1 file2\n");
            return 2;
        }
    }
    if (argc - optind != 2) {
        eprintf("usage: cmp [-s] file1 file2\n");
        return 2;
    }
    const char* first = argv[optind];
    const char* second = argv[optind + 1];
    int fd1 = open(first, O_RDONLY);
    if (fd1 < 0) { eprintf("cmp: %s: cannot open\n", first); return 2; }
    int fd2 = open(second, O_RDONLY);
    if (fd2 < 0) {
        eprintf("cmp: %s: cannot open\n", second);
        (void)close(fd1);
        return 2;
    }
    struct rbuf r1, r2;
    rb_init(&r1, fd1);
    rb_init(&r2, fd2);
    unsigned long offset = 1, line = 1;
    int status = 0;
    for (;;) {
        int a = rb_getc(&r1);
        int b = rb_getc(&r2);
        if (a < 0 && b < 0) break;
        if (a < 0 || b < 0) {
            if (!silent)
                eprintf("cmp: EOF on %s after byte %lu\n", a < 0 ? first : second, offset - 1);
            status = 1;
            break;
        }
        if (a != b) {
            if (!silent)
                printf("%s %s differ: byte %lu, line %lu\n", first, second, offset, line);
            status = 1;
            break;
        }
        if (a == '\n') line++;
        offset++;
    }
    (void)close(fd1);
    (void)close(fd2);
    return status;
}
