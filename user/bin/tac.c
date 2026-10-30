#include <tach.h>
#include <tachc.h>

/* tac [arquivo...]: imprime as linhas de tras para frente. */

static char* g_data;
static size_t g_len, g_cap;

static int read_all(int fd) {
    for (;;) {
        if (g_cap - g_len < 1025) {
            size_t new_cap = g_cap ? g_cap * 2 : 4096;
            char* grown = (char*)realloc(g_data, new_cap);
            if (!grown) return -1;
            g_data = grown;
            g_cap = new_cap;
        }
        ssize_t count = read(fd, g_data + g_len, g_cap - g_len - 1);
        if (count < 0) return -1;
        if (count == 0) return 0;
        g_len += (size_t)count;
    }
}

int main(int argc, char** argv) {
    int status = 0;
    if (argc < 2) {
        if (read_all(0) < 0) status = 1;
    }
    for (int i = 1; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("tac: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        if (read_all(fd) < 0) {
            eprintf("tac: %s: read error or out of memory\n", argv[i]);
            status = 1;
        }
        if (fd != 0) (void)close(fd);
    }
    if (!g_len) return status;

    /* Anda de tras para frente; cada linha inclui o seu '\n' final. Se o
     * texto nao termina em '\n', a ultima linha sai com um '\n' acrescentado. */
    size_t end = g_len;
    while (end > 0) {
        size_t start = end - 1;
        if (g_data[start] == '\n' && start > 0) start--;
        while (start > 0 && g_data[start - 1] != '\n') start--;
        if (write_all(1, g_data + start, end - start) < 0) return 1;
        if (g_data[end - 1] != '\n' && write_all(1, "\n", 1) < 0) return 1;
        end = start;
    }
    return status;
}
