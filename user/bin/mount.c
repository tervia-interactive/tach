#include <tach.h>
#include <tachc.h>

/* mount dispositivo diretorio [tipo]   (ex.: mount /dev/hda1 /mnt fat32)
 * Sem argumentos: lista as montagens (le /proc/mounts). */
int main(int argc, char** argv) {
    if (argc == 1) { /* sem argumentos: lista as montagens */
        int fd = open("/proc/mounts", O_RDONLY);
        if (fd < 0) {
            eprintf("mount: /proc/mounts unavailable\n");
            return 1;
        }
        char buf[256];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) (void)write_all(1, buf, (size_t)n);
        (void)close(fd);
        return 0;
    }
    if (argc < 3) {
        eprintf("usage: mount [/dev/<disk> <dir> [fat32]]\n");
        return 2;
    }
    if (mount(argv[1], argv[2], argc > 3 ? argv[3] : "fat32") < 0) {
        eprintf("mount: cannot mount %s on %s (root only; needs a FAT32 volume)\n", argv[1], argv[2]);
        return 1;
    }
    return 0;
}
