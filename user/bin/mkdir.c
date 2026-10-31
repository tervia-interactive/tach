#include <tach.h>
#include <tachc.h>

/* mkdir dir... */
int main(int argc, char** argv) {
    if (argc < 2) {
        eprintf("usage: mkdir dir...\n");
        return 2;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i], 0755) < 0) {
            eprintf("mkdir: %s: cannot create directory\n", argv[i]);
            status = 1;
        }
    }
    return status;
}
