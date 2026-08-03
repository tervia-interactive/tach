#include <tach.h>
int main(int argc, char** argv) {
    if (argc != 2) { puts_fd(2, "usage: touch FILE\n"); return 1; }
    int fd = open(argv[1], O_WRONLY | O_CREAT);
    if (fd < 0) { puts_fd(2, "touch: failed\n"); return 1; }
    close(fd); return 0;
}
