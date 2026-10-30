#include <tach.h>
#include <tachc.h>

/* stat arquivo...: mostra tipo, tamanho, modo, inode, links, uid e gid. */

static const char* type_name(unsigned int mode) {
    if (S_ISDIR(mode)) return "directory";
    if (S_ISCHR(mode)) return "character device";
    if (S_ISFIFO(mode)) return "fifo";
    if (S_ISREG(mode)) return "regular file";
    return "other";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        eprintf("usage: stat file...\n");
        return 2;
    }
    int status = 0;
    struct stat info;
    for (int i = 1; i < argc; i++) {
        if (stat(argv[i], &info) < 0) {
            eprintf("stat: %s: no such file\n", argv[i]);
            status = 1;
            continue;
        }
        printf("  File: %s\n", argv[i]);
        printf("  Size: %llu\tType: %s\n", info.st_size, type_name(info.st_mode));
        printf("  Mode: %04o\tInode: %u\tLinks: %u\n", info.st_mode & 07777u,
               info.st_ino, info.st_nlink);
        printf("   Uid: %u\tGid: %u\n", info.st_uid, info.st_gid);
    }
    return status;
}
