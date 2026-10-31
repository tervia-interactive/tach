#include <tach.h>
#include <tachc.h>

/* touch arquivo...: cria arquivos vazios que ainda nao existem. Nao ha
 * syscall de utimes, entao arquivos existentes ficam intactos (sem truncar).
 * Binario externo para uso com xargs; o shell tambem tem um builtin. */
int main(int argc, char** argv) {
    if (argc < 2) {
        eprintf("usage: touch file...\n");
        return 2;
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        struct stat info;
        if (stat(argv[i], &info) == 0) continue;
        int fd = open(argv[i], O_WRONLY | O_CREAT);
        if (fd < 0) {
            eprintf("touch: %s: cannot create\n", argv[i]);
            status = 1;
            continue;
        }
        (void)close(fd);
    }
    return status;
}
