#include <tach.h>
int main(int argc, char** argv) {
    if (argc < 3) { puts_fd(2, "usage: write FILE TEXT...\n"); return 1; }
    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { puts_fd(2, "write: cannot open file\n"); return 1; }
    for (int i = 2; i < argc; i++) {
        if (i > 2) (void)write(fd, " ", 1);
        (void)write(fd, argv[i], strlen(argv[i]));
    }
    (void)write(fd, "\n", 1); close(fd); (void)sync(); return 0;
}
