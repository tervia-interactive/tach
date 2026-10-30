#include <tach.h>
#include <tachc.h>

/* echo [-n] [args...]: versao externa (usada por xargs e pipelines). */

int main(int argc, char** argv) {
    int newline = 1;
    int first = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        newline = 0;
        first = 2;
    }
    for (int i = first; i < argc; i++) {
        if (i > first) puts_fd(1, " ");
        puts_fd(1, argv[i]);
    }
    if (newline) puts_fd(1, "\n");
    return 0;
}
