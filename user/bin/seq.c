#include <tach.h>
#include <tachc.h>

/* seq [-s sep] [primeiro [passo]] ultimo  (inteiros com sinal) */

static int parse(const char* text, long* out) {
    char* end;
    *out = strtol(text, &end, 10);
    return text[0] != '\0' && *end == '\0';
}

int main(int argc, char** argv) {
    const char* separator = "\n";
    int c;
    while ((c = getopt(argc, argv, "s:")) != -1) {
        if (c == 's') separator = optarg;
        else {
            eprintf("usage: seq [-s sep] [first [step]] last\n");
            return 2;
        }
    }
    long first = 1, step = 1, last = 0;
    int count = argc - optind;
    if (count < 1 || count > 3) {
        eprintf("usage: seq [-s sep] [first [step]] last\n");
        return 2;
    }
    int ok;
    if (count == 1) ok = parse(argv[optind], &last);
    else if (count == 2) ok = parse(argv[optind], &first) && parse(argv[optind + 1], &last);
    else ok = parse(argv[optind], &first) && parse(argv[optind + 1], &step) &&
              parse(argv[optind + 2], &last);
    if (!ok) {
        eprintf("seq: invalid number\n");
        return 2;
    }
    if (step == 0) {
        eprintf("seq: step must not be zero\n");
        return 2;
    }
    int printed = 0;
    for (long value = first; step > 0 ? value <= last : value >= last;) {
        if (printed) puts_fd(1, separator);
        printf("%ld", value);
        printed = 1;
        long next = value + step;
        /* para no overflow */
        if ((step > 0 && next < value) || (step < 0 && next > value)) break;
        value = next;
    }
    if (printed) puts_fd(1, "\n");
    return 0;
}
