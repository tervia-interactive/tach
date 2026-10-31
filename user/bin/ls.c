#include <tach.h>
#include <tachc.h>

/* ls [-l] [-a] [-1] [caminho...]: lista diretorios em ordem alfabetica.
 * Binario externo (para xargs/pipelines); o shell tem um builtin ls sem
 * ordenacao. Sem -a, nomes que comecam com '.' sao omitidos. */

#define MAX_ENTRIES 1024
#define PATH_CAP 256

static char* g_names[MAX_ENTRIES];
static size_t g_count;
static int g_long, g_all;
static struct wbuf g_writer;

static int by_name(const void* left, const void* right) {
    return strcmp(*(char* const*)left, *(char* const*)right);
}

static void mode_text(unsigned int mode, char out[11]) {
    out[0] = S_ISDIR(mode) ? 'd' : S_ISCHR(mode) ? 'c' : '-';
    for (int i = 0; i < 9; i++)
        out[1 + i] = (mode & (0400u >> i)) ? "rwx"[i % 3] : '-';
    out[10] = '\0';
}

static void print_entry(const char* name, unsigned int mode, unsigned long size) {
    char line[PATH_CAP + 40];
    int n;
    if (g_long) {
        char text[11];
        mode_text(mode, text);
        n = snprintf(line, sizeof(line), "%s %8lu %s%s\n", text, size, name,
                     S_ISDIR(mode) ? "/" : "");
    } else {
        n = snprintf(line, sizeof(line), "%s%s\n", name, S_ISDIR(mode) ? "/" : "");
    }
    if (n > 0) wb_write(&g_writer, line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1);
}

static void free_names(void) {
    for (size_t i = 0; i < g_count; i++) free(g_names[i]);
    g_count = 0;
}

static int list_path(const char* path) {
    struct stat info;
    if (stat(path, &info) < 0) {
        eprintf("ls: %s: no such file or directory\n", path);
        return 1;
    }
    if (!S_ISDIR(info.st_mode)) {
        print_entry(path, info.st_mode, (unsigned long)info.st_size);
        return 0;
    }
    int fd = open(path, O_RDONLY);
    struct dirent* entry = fd >= 0 ? (struct dirent*)malloc(sizeof(*entry)) : 0;
    if (!entry) {
        if (fd >= 0) (void)close(fd);
        eprintf("ls: %s: cannot open\n", path);
        return 1;
    }
    int status = 0;
    g_count = 0;
    for (;;) {
        ssize_t bytes = getdents(fd, entry, sizeof(*entry));
        if (bytes <= 0) break;
        if (!g_all && entry->d_name[0] == '.') continue;
        if (g_count == MAX_ENTRIES) {
            eprintf("ls: %s: too many entries (showing first %d)\n", path, MAX_ENTRIES);
            status = 1;
            break;
        }
        char* copy = strdup(entry->d_name);
        if (!copy) { status = 1; break; }
        g_names[g_count] = copy;
        g_count++;
    }
    free(entry);
    (void)close(fd);

    /* ordena so os nomes; modo e tamanho vem de stat na hora de imprimir. */
    qsort(g_names, g_count, sizeof(char*), by_name);
    size_t length = strlen(path);
    for (size_t i = 0; i < g_count; i++) {
        unsigned int mode = S_IFREG;
        unsigned long size = 0;
        char full[PATH_CAP];
        size_t name_length = strlen(g_names[i]);
        if (length + 1 + name_length < sizeof(full)) {
            memcpy(full, path, length);
            size_t position = length;
            if (position == 0 || full[position - 1] != '/') full[position++] = '/';
            memcpy(full + position, g_names[i], name_length + 1);
            struct stat child;
            if (stat(full, &child) == 0) {
                mode = child.st_mode;
                size = (unsigned long)child.st_size;
            }
        }
        print_entry(g_names[i], mode, size);
    }
    free_names();
    return status;
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "la1")) != -1) {
        if (c == 'l') g_long = 1;
        else if (c == 'a') g_all = 1;
        else if (c == '1') continue;
        else {
            eprintf("usage: ls [-l] [-a] [-1] [path...]\n");
            return 2;
        }
    }
    wb_init(&g_writer, 1);
    int status = 0;
    if (optind >= argc) {
        status = list_path(".");
    } else {
        int several = argc - optind > 1;
        for (int i = optind; i < argc; i++) {
            if (several) {
                char head[PATH_CAP + 4];
                int n = snprintf(head, sizeof(head), "%s%s:\n", i > optind ? "\n" : "", argv[i]);
                if (n > 0) wb_write(&g_writer, head, (size_t)n < sizeof(head) ? (size_t)n : sizeof(head) - 1);
            }
            if (list_path(argv[i])) status = 1;
        }
    }
    if (wb_flush(&g_writer) < 0) status = 1;
    return status;
}
