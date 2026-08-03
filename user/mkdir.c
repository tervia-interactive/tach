#include <tach.h>
int main(int argc, char** argv) {
    if (argc != 2) { puts_fd(2, "usage: mkdir DIRECTORY\n"); return 1; }
    if (mkdir(argv[1], 0755) < 0) { puts_fd(2, "mkdir: failed\n"); return 1; }
    return 0;
}
