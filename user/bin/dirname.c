#include <tach.h>
#include <tachc.h>

/* dirname caminho */

static char g_path[256];

int main(int argc, char** argv) {
    if (argc != 2) {
        eprintf("usage: dirname path\n");
        return 2;
    }
    if (strlcpy(g_path, argv[1], sizeof(g_path)) >= sizeof(g_path)) {
        eprintf("dirname: path too long\n");
        return 1;
    }
    size_t length = strlen(g_path);
    while (length > 1 && g_path[length - 1] == '/') g_path[--length] = '\0';
    char* slash = strrchr(g_path, '/');
    if (!slash) {
        puts_fd(1, ".\n");
        return 0;
    }
    while (slash > g_path && slash[-1] == '/') slash--;
    if (slash == g_path) {
        puts_fd(1, "/\n");
        return 0;
    }
    *slash = '\0';
    puts_fd(1, g_path);
    puts_fd(1, "\n");
    return 0;
}
