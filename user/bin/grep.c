#include <tach.h>
#include <tachc.h>

/* grep [-ivcnlqFHh] [-e padrao] padrao [arquivo...]
 * Padrao: regex simples (^ $ . * + ? [..] \c) ou texto fixo com -F.
 * Linhas com mais de 1023 bytes sao truncadas antes da comparacao.
 * Saida 0 = houve casamento, 1 = nenhum, 2 = erro. */

#define LINE_CAP 1024

static char g_line[LINE_CAP];
static struct rbuf g_reader;

struct options {
    int icase, invert, count_only, number, list_files, quiet, fixed;
    int show_name;
};

/* Devolve o numero de linhas selecionadas, ou -1 em erro. */
static long grep_fd(int fd, const char* pattern, const struct options* opt,
                    const char* name) {
    rb_init(&g_reader, fd);
    long selected = 0;
    unsigned long line_number = 0;
    for (;;) {
        long length = rb_getline(&g_reader, g_line, sizeof(g_line));
        if (length < 0) break;
        line_number++;
        int hit = opt->fixed ? str_find(g_line, pattern, opt->icase) != (const char*)0
                             : re_match(pattern, g_line, opt->icase);
        if (opt->invert) hit = !hit;
        if (!hit) continue;
        selected++;
        if (opt->quiet || opt->list_files) break;
        if (opt->count_only) continue;
        if (opt->show_name) printf("%s:", name);
        if (opt->number) printf("%lu:", line_number);
        printf("%s\n", g_line);
    }
    return selected;
}

int main(int argc, char** argv) {
    struct options opt;
    memset(&opt, 0, sizeof(opt));
    const char* pattern = (const char*)0;
    int force_name = 0, force_no_name = 0;
    int c;
    while ((c = getopt(argc, argv, "ivcnlqFHhe:")) != -1) {
        switch (c) {
        case 'i': opt.icase = 1; break;
        case 'v': opt.invert = 1; break;
        case 'c': opt.count_only = 1; break;
        case 'n': opt.number = 1; break;
        case 'l': opt.list_files = 1; break;
        case 'q': opt.quiet = 1; break;
        case 'F': opt.fixed = 1; break;
        case 'H': force_name = 1; break;
        case 'h': force_no_name = 1; break;
        case 'e': pattern = optarg; break;
        default:
            eprintf("usage: grep [-ivcnlqFHh] [-e pattern] pattern [file...]\n");
            return 2;
        }
    }
    if (!pattern) {
        if (optind >= argc) {
            eprintf("usage: grep [-ivcnlqFHh] [-e pattern] pattern [file...]\n");
            return 2;
        }
        pattern = argv[optind++];
    }
    int files = argc - optind;
    opt.show_name = force_name || (files > 1 && !force_no_name);

    if (files <= 0) {
        long n = grep_fd(0, pattern, &opt, "(stdin)");
        if (n < 0) return 2;
        if (opt.count_only && !opt.quiet) printf("%ld\n", n);
        return n > 0 ? 0 : 1;
    }

    int matched = 0, error = 0;
    for (int i = optind; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("grep: %s: cannot open\n", argv[i]);
            error = 1;
            continue;
        }
        long n = grep_fd(fd, pattern, &opt, argv[i]);
        if (fd != 0) (void)close(fd);
        if (n < 0) { error = 1; continue; }
        if (n > 0) matched = 1;
        if (opt.quiet && matched) return 0;
        if (opt.count_only) {
            if (opt.show_name) printf("%s:", argv[i]);
            printf("%ld\n", n);
        } else if (opt.list_files && n > 0) {
            printf("%s\n", argv[i]);
        }
    }
    if (error) return 2;
    return matched ? 0 : 1;
}
