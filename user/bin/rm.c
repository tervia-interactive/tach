#include <tach.h>
#include <tachc.h>

/* rm [-r] [-f] arquivo...: remove arquivos; com -r remove diretorios de forma
 * recursiva; com -f ignora inexistentes e nao falha por isso.
 *
 * Existe como binario externo para funcionar em pipelines como
 * `find /tmp/x | xargs rm` (xargs so procura comandos em /bin); o shell tem
 * um builtin rm simples que continua valendo na linha de comando.
 *
 * Remover uma entrada durante getdents pode fazer o driver pular a seguinte,
 * entao a varredura recomeca do inicio do diretorio depois de cada remocao. */

#define PATH_CAP 256
#define DEPTH_LIMIT 16

static char g_path[PATH_CAP];
static int g_recursive, g_force;

static const char* reason(int code) {
    if (code == -16) return "file is in use";
    if (code == -30) return "read-only file system";
    return "cannot remove";
}

/* Copia para name o primeiro filho (diferente de . e ..) de g_path.
 * Retorna 1 se achou, 0 se vazio, -1 em erro. */
static int first_child(char* name, size_t cap) {
    int fd = open(g_path, O_RDONLY);
    if (fd < 0) return -1;
    struct dirent* entry = (struct dirent*)malloc(sizeof(*entry));
    if (!entry) { (void)close(fd); return -1; }
    int found = 0;
    for (;;) {
        ssize_t bytes = getdents(fd, entry, sizeof(*entry));
        if (bytes <= 0) break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (strlen(entry->d_name) >= cap) { found = -1; break; }
        strcpy(name, entry->d_name);
        found = 1;
        break;
    }
    free(entry);
    (void)close(fd);
    return found;
}

static int remove_path(size_t length, int depth) {
    struct stat info;
    if (stat(g_path, &info) < 0) {
        if (g_force) return 0;
        eprintf("rm: %s: no such file or directory\n", g_path);
        return 1;
    }
    if (!S_ISDIR(info.st_mode)) {
        int result = unlink(g_path);
        if (result < 0) {
            eprintf("rm: %s: %s\n", g_path, reason(result));
            return 1;
        }
        return 0;
    }
    if (!g_recursive) {
        eprintf("rm: %s: is a directory (use -r)\n", g_path);
        return 1;
    }
    if (depth >= DEPTH_LIMIT) {
        eprintf("rm: %s: directory nesting too deep\n", g_path);
        return 1;
    }
    int status = 0;
    char name[PATH_CAP];
    for (;;) {
        int found = first_child(name, sizeof(name));
        if (found < 0) {
            eprintf("rm: %s: cannot read directory\n", g_path);
            return 1;
        }
        if (found == 0) break;
        size_t position = length;
        size_t name_length = strlen(name);
        if (position == 0 || g_path[position - 1] != '/') g_path[position++] = '/';
        if (position + name_length >= sizeof(g_path)) {
            g_path[length] = '\0';
            eprintf("rm: %s: path too long\n", g_path);
            return 1;
        }
        memcpy(g_path + position, name, name_length + 1);
        int child = remove_path(position + name_length, depth + 1);
        g_path[length] = '\0';
        if (child) {
            /* a entrada continua la: sair evita laco infinito */
            return 1;
        }
    }
    int result = rmdir(g_path);
    if (result < 0) {
        eprintf("rm: %s: cannot remove directory\n", g_path);
        status = 1;
    }
    return status;
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "rRf")) != -1) {
        if (c == 'r' || c == 'R') g_recursive = 1;
        else if (c == 'f') g_force = 1;
        else {
            eprintf("usage: rm [-r] [-f] file...\n");
            return 2;
        }
    }
    if (optind >= argc) {
        if (g_force) return 0;
        eprintf("usage: rm [-r] [-f] file...\n");
        return 2;
    }
    int status = 0;
    for (int i = optind; i < argc; i++) {
        size_t length = strlen(argv[i]);
        if (length >= sizeof(g_path)) {
            eprintf("rm: %s: path too long\n", argv[i]);
            status = 1;
            continue;
        }
        memcpy(g_path, argv[i], length + 1);
        /* nunca apagar a raiz */
        if (strcmp(g_path, "/") == 0) {
            eprintf("rm: refusing to remove '/'\n");
            status = 1;
            continue;
        }
        if (remove_path(length, 0)) status = 1;
    }
    return status;
}
