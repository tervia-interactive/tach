#include <tach.h>
#include <tachc.h>

/* rmdir dir... (so diretorios vazios) */
int main(int argc, char** argv) {
    if (argc < 2) {
        eprintf("usage: rmdir dir...\n");
        return 2;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            eprintf("rmdir: %s: cannot remove (missing, not a directory, not empty or busy)\n", argv[i]);
            status = 1;
        }
    }
    return status;
}
