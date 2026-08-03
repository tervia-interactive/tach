#include <tach.h>
int main(int argc, char** argv) {
    if (argc != 3) { puts_fd(2, "usage: cp SOURCE DESTINATION\n"); return 1; }
    int source = open(argv[1], O_RDONLY);
    int destination = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC);
    if (source < 0 || destination < 0) {
        puts_fd(2, "cp: cannot open file\n");
        if (source >= 0) close(source);
        if (destination >= 0) close(destination);
        return 1;
    }
    char buffer[256];
    for (;;) {
        ssize_t bytes = read(source, buffer, sizeof(buffer));
        if (bytes < 0) { puts_fd(2, "cp: read failed\n"); return 1; }
        if (!bytes) break;
        if (write(destination, buffer, (size_t)bytes) != bytes) {
            puts_fd(2, "cp: write failed\n"); return 1;
        }
    }
    close(source); close(destination); (void)sync(); return 0;
}
