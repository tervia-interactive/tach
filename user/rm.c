#include <tach.h>
int main(int argc, char** argv) {
    if (argc != 2) { puts_fd(2, "usage: rm PATH\n"); return 1; }
    if (unlink(argv[1]) < 0) { puts_fd(2, "rm: failed\n"); return 1; }
    return 0;
}
