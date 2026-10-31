#include <tach.h>
#include <tachc.h>

/* umount dir */
int main(int argc, char** argv) {
    if (argc != 2) {
        eprintf("usage: umount <dir>\n");
        return 2;
    }
    if (umount(argv[1]) < 0) {
        eprintf("umount: %s: failed (not a mount point, busy or not root)\n", argv[1]);
        return 1;
    }
    return 0;
}
