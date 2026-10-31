#include <tach.h>
#include <tachc.h>

/* mount dispositivo diretorio [tipo]   (ex.: mount /dev/hda1 /mnt fat32)
 * Sem argumentos: ainda nao lista a tabela (falta syscall para isso). */
int main(int argc, char** argv) {
    if (argc < 3) {
        eprintf("usage: mount /dev/<disk> <dir> [fat32]\n");
        return 2;
    }
    if (mount(argv[1], argv[2], argc > 3 ? argv[3] : "fat32") < 0) {
        eprintf("mount: cannot mount %s on %s (root only; needs a FAT32 volume)\n", argv[1], argv[2]);
        return 1;
    }
    return 0;
}
