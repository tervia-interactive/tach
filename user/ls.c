#include <tach.h>
int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/";
    int fd = open(path, O_RDONLY);
    if (fd < 0) { puts_fd(2, "ls: cannot open path\n"); return 1; }
    struct dirent entries[4];
    for (;;) {
        int bytes = getdents(fd, entries, sizeof(entries));
        if (bytes < 0) { puts_fd(2, "ls: not a directory\n"); close(fd); return 1; }
        if (!bytes) break;
        for (size_t offset = 0; offset + sizeof(struct dirent) <= (size_t)bytes;
             offset += sizeof(struct dirent)) {
            struct dirent* entry = (struct dirent*)((char*)entries + offset);
            puts_fd(1, entry->d_name);
            if (entry->d_type == VNODE_DIR) puts_fd(1, "/");
            puts_fd(1, "\n");
        }
    }
    close(fd); return 0;
}
