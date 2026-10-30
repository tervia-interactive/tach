#include <tach.h>
#include <tachc.h>

/* basename caminho [sufixo] */

static char g_path[256];

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        eprintf("usage: basename path [suffix]\n");
        return 2;
    }
    if (strlcpy(g_path, argv[1], sizeof(g_path)) >= sizeof(g_path)) {
        eprintf("basename: path too long\n");
        return 1;
    }
    size_t length = strlen(g_path);
    while (length > 1 && g_path[length - 1] == '/') g_path[--length] = '\0';
    const char* name = g_path;
    if (!(length == 1 && g_path[0] == '/')) name = path_basename(g_path);
    size_t name_length = strlen(name);
    if (argc == 3) {
        size_t suffix = strlen(argv[2]);
        if (suffix && suffix < name_length &&
            strcmp(name + name_length - suffix, argv[2]) == 0)
            name_length -= suffix;
    }
    (void)write_all(1, name, name_length);
    (void)write_all(1, "\n", 1);
    return 0;
}
