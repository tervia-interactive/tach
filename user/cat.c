#include <tach.h>
int main(int argc, char** argv) {
    if (argc < 2) { puts_fd(2, "usage: cat FILE...\n"); return 1; }
    char buffer[256]; int status = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) { puts_fd(2, "cat: cannot open file\n"); status = 1; continue; }
        for (;;) {
            ssize_t bytes = read(fd, buffer, sizeof(buffer));
            if (bytes < 0) { puts_fd(2, "cat: read failed\n"); status = 1; break; }
            if (!bytes) break;
            (void)write(1, buffer, (size_t)bytes);
        }
        close(fd);
    }
    return status;
}
