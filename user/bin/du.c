#include <tach.h>
#include <tachc.h>

/* du [-s] [-b] [caminho...]: soma de tamanhos de arquivo (nao de blocos).
 * Padrao: KiB arredondado para cima, uma linha por diretorio; -s so o total;
 * -b em bytes. */

#define PATH_CAP 256
#define DEPTH_LIMIT 16

static char g_path[PATH_CAP];
static int g_summary, g_bytes;
static struct wbuf g_writer;

static void show(unsigned long bytes, const char* path) {
    char line[PATH_CAP + 24];
    unsigned long value = g_bytes ? bytes : (bytes + 1023u) / 1024u;
    int n = snprintf(line, sizeof(line), "%lu\t%s\n", value, path);
    if (n > 0) wb_write(&g_writer, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1);
}

static unsigned long walk(size_t length, int depth, int top) {
    struct stat info;
    if (stat(g_path, &info) < 0) {
        eprintf("du: %s: cannot stat\n", g_path);
        return 0;
    }
    unsigned long total = (unsigned long)info.st_size;
    if (!S_ISDIR(info.st_mode)) {
        if (top) show(total, g_path);
        return total;
    }
    total = 0;
    if (depth < DEPTH_LIMIT) {
        int fd = open(g_path, O_RDONLY);
        struct dirent* entry = fd >= 0 ? (struct dirent*)malloc(sizeof(*entry)) : 0;
        if (fd >= 0 && entry) {
            for (;;) {
                ssize_t bytes = getdents(fd, entry, sizeof(*entry));
                if (bytes <= 0) break;
                const char* name = entry->d_name;
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
                size_t name_length = strlen(name);
                size_t position = length;
                if (position == 0 || g_path[position - 1] != '/') g_path[position++] = '/';
                if (position + name_length >= sizeof(g_path)) {
                    g_path[length] = '\0';
                    continue;
                }
                memcpy(g_path + position, name, name_length + 1);
                total += walk(position + name_length, depth + 1, 0);
                g_path[length] = '\0';
            }
        }
        if (entry) free(entry);
        if (fd >= 0) (void)close(fd);
    }
    if (!g_summary || top) show(total, g_path);
    return total;
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "sb")) != -1) {
        if (c == 's') g_summary = 1;
        else if (c == 'b') g_bytes = 1;
        else {
            eprintf("usage: du [-s] [-b] [path...]\n");
            return 2;
        }
    }
    wb_init(&g_writer, 1);
    int count = argc - optind;
    int status = 0;
    for (int i = 0; i < (count ? count : 1); i++) {
        const char* root = count ? argv[optind + i] : ".";
        size_t length = strlen(root);
        if (length >= sizeof(g_path)) {
            eprintf("du: path too long\n");
            status = 1;
            continue;
        }
        memcpy(g_path, root, length + 1);
        while (length > 1 && g_path[length - 1] == '/') g_path[--length] = '\0';
        (void)walk(length, 0, 1);
    }
    if (wb_flush(&g_writer) < 0) status = 1;
    return status;
}
