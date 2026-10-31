#include <tach.h>
#include <tachc.h>

/* mkdir [-p] dir...   (-p: cria os pais que faltam e nao falha se ja existe) */

static char g_path[256];

static int is_dir(const char* path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int make_parents(const char* path) {
    size_t length = strlen(path);
    if (!length || length >= sizeof(g_path)) return -1;
    memcpy(g_path, path, length + 1);
    for (size_t i = 1; i <= length; i++) {
        if (g_path[i] != '/' && g_path[i] != '\0') continue;
        char saved = g_path[i];
        g_path[i] = '\0';
        if (!is_dir(g_path) && mkdir(g_path, 0755) < 0) return -1;
        g_path[i] = saved;
    }
    return 0;
}

int main(int argc, char** argv) {
    int parents = 0;
    int c;
    while ((c = getopt(argc, argv, "p")) != -1) {
        if (c == 'p') parents = 1;
        else {
            eprintf("usage: mkdir [-p] dir...\n");
            return 2;
        }
    }
    if (optind >= argc) {
        eprintf("usage: mkdir [-p] dir...\n");
        return 2;
    }
    int status = 0;
    for (int i = optind; i < argc; i++) {
        int result = parents ? make_parents(argv[i]) : mkdir(argv[i], 0755);
        if (result < 0) {
            eprintf("mkdir: %s: cannot create directory\n", argv[i]);
            status = 1;
        }
    }
    return status;
}
