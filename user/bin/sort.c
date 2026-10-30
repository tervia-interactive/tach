#include <tach.h>
#include <tachc.h>

/* sort [-rnfu] [arquivo...]: ordena linhas (estavel). -r inverte, -n numerico,
 * -f ignora caixa, -u remove linhas iguais. Le tudo para a memoria (heap). */

static char* g_data;
static size_t g_len, g_cap;
static char** g_lines;
static size_t g_count;
static int g_rev, g_num, g_fold;

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

static int key_compare(const char* x, const char* y) {
    if (g_num) {
        long nx = strtol(x, (char**)0, 10);
        long ny = strtol(y, (char**)0, 10);
        return nx < ny ? -1 : (nx > ny ? 1 : 0);
    }
    return g_fold ? strcasecmp(x, y) : strcmp(x, y);
}

static int line_compare(const void* a, const void* b) {
    const char* x = *(char* const*)a;
    const char* y = *(char* const*)b;
    int result = key_compare(x, y);
    if (g_rev) result = -result;
    if (result == 0) result = x < y ? -1 : (x > y ? 1 : 0); /* estavel */
    return result;
}

int main(int argc, char** argv) {
    int unique = 0;
    int c;
    while ((c = getopt(argc, argv, "rnfu")) != -1) {
        if (c == 'r') g_rev = 1;
        else if (c == 'n') g_num = 1;
        else if (c == 'f') g_fold = 1;
        else if (c == 'u') unique = 1;
        else {
            eprintf("usage: sort [-rnfu] [file...]\n");
            return 2;
        }
    }
    int status = 0;
    if (optind >= argc) {
        if (read_all(0) < 0) status = 1;
    }
    for (int i = optind; i < argc; i++) {
        int fd = strcmp(argv[i], "-") == 0 ? 0 : open(argv[i], O_RDONLY);
        if (fd < 0) {
            eprintf("sort: %s: cannot open\n", argv[i]);
            status = 1;
            continue;
        }
        if (read_all(fd) < 0) {
            eprintf("sort: %s: read error or out of memory\n", argv[i]);
            status = 1;
        }
        if (fd != 0) (void)close(fd);
    }
    if (!g_data || !g_len) return status;
    g_data[g_len] = '\0';

    size_t slots = 1;
    for (size_t i = 0; i < g_len; i++)
        if (g_data[i] == '\n') slots++;
    g_lines = (char**)calloc(slots, sizeof(char*));
    if (!g_lines) {
        eprintf("sort: out of memory\n");
        return 1;
    }
    size_t position = 0;
    while (position < g_len) {
        g_lines[g_count++] = g_data + position;
        char* newline = (char*)memchr(g_data + position, '\n', g_len - position);
        if (!newline) break;
        *newline = '\0';
        position = (size_t)(newline - g_data) + 1;
    }
    qsort(g_lines, g_count, sizeof(char*), line_compare);

    static struct wbuf writer;
    wb_init(&writer, 1);
    for (size_t i = 0; i < g_count; i++) {
        if (unique && i > 0 && key_compare(g_lines[i - 1], g_lines[i]) == 0)
            continue;
        wb_write(&writer, g_lines[i], strlen(g_lines[i]));
        wb_putc(&writer, '\n');
    }
    if (wb_flush(&writer) < 0) status = 1;
    return status;
}
