#include <tach.h>
#include <tachc.h>

/* find [caminho...] [-name GLOB] [-type f|d] [-maxdepth N]
 * Percorre diretorios com getdents. GLOB aceita * ? [..]. */

#define PATH_CAP 256
#define DEPTH_LIMIT 16

static char g_path[PATH_CAP];
static const char* g_name_glob;
static char g_type; /* 0 = qualquer, 'f' ou 'd' */
static int g_max_depth = DEPTH_LIMIT;
static struct wbuf g_writer;

static int glob_match(const char* pattern, const char* text) {
    const char* star_pattern = (const char*)0;
    const char* star_text = (const char*)0;
    while (*text) {
        if (*pattern == '*') {
            star_pattern = ++pattern;
            star_text = text;
            continue;
        }
        int ok = 0;
        const char* next = pattern + 1;
        if (*pattern == '?') {
            ok = 1;
        } else if (*pattern == '[') {
            const char* p = pattern + 1;
            int negate = (*p == '!' || *p == '^');
            if (negate) p++;
            const char* close = strchr(*p == ']' ? p + 1 : p, ']');
            if (close) {
                int found = 0;
                for (; p < close; p++) {
                    if (p + 2 < close && p[1] == '-') {
                        if (*text >= p[0] && *text <= p[2]) found = 1;
                        p += 2;
                    } else if (*p == *text) {
                        found = 1;
                    }
                }
                ok = (found != negate);
                next = close + 1;
            } else {
                ok = (*pattern == *text);
            }
        } else if (*pattern && *pattern == *text) {
            ok = 1;
        }
        if (ok) {
            pattern = next;
            text++;
        } else if (star_pattern) {
            pattern = star_pattern;
            text = ++star_text;
        } else {
            return 0;
        }
    }
    while (*pattern == '*') pattern++;
    return *pattern == '\0';
}

static void report(unsigned int mode) {
    if (g_type == 'f' && !S_ISREG(mode)) return;
    if (g_type == 'd' && !S_ISDIR(mode)) return;
    if (g_name_glob) {
        const char* name = path_basename(g_path);
        if (!*name) name = g_path; /* "/" */
        if (!glob_match(g_name_glob, name)) return;
    }
    wb_write(&g_writer, g_path, strlen(g_path));
    wb_putc(&g_writer, '\n');
}

static void walk(size_t length, int depth) {
    struct stat info;
    if (stat(g_path, &info) < 0) {
        eprintf("find: %s: cannot stat\n", g_path);
        return;
    }
    report(info.st_mode);
    if (!S_ISDIR(info.st_mode) || depth >= g_max_depth) return;

    int fd = open(g_path, O_RDONLY);
    if (fd < 0) {
        eprintf("find: %s: cannot open\n", g_path);
        return;
    }
    struct dirent* entry = (struct dirent*)malloc(sizeof(struct dirent));
    if (!entry) {
        (void)close(fd);
        return;
    }
    for (;;) {
        ssize_t bytes = getdents(fd, entry, sizeof(struct dirent));
        if (bytes <= 0) break;
        const char* name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        size_t name_length = strlen(name);
        size_t position = length;
        if (position == 0 || g_path[position - 1] != '/') g_path[position++] = '/';
        if (position + name_length >= sizeof(g_path)) {
            g_path[length] = '\0';
            eprintf("find: path too long under %s\n", g_path);
            continue;
        }
        memcpy(g_path + position, name, name_length + 1);
        walk(position + name_length, depth + 1);
        g_path[length] = '\0';
    }
    free(entry);
    (void)close(fd);
}

int main(int argc, char** argv) {
    int paths_end = argc;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') { paths_end = i; break; }
    }
    for (int i = paths_end; i < argc; i += 2) {
        if (i + 1 >= argc) {
            eprintf("find: missing argument to %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "-name") == 0) {
            g_name_glob = argv[i + 1];
        } else if (strcmp(argv[i], "-type") == 0) {
            char t = argv[i + 1][0];
            if ((t != 'f' && t != 'd') || argv[i + 1][1]) {
                eprintf("find: -type takes f or d\n");
                return 2;
            }
            g_type = t;
        } else if (strcmp(argv[i], "-maxdepth") == 0) {
            long n = strtol(argv[i + 1], (char**)0, 10);
            if (n < 0 || n > DEPTH_LIMIT) {
                eprintf("find: -maxdepth must be 0..%d\n", DEPTH_LIMIT);
                return 2;
            }
            g_max_depth = (int)n;
        } else {
            eprintf("find: unknown option %s\n", argv[i]);
            return 2;
        }
    }
    wb_init(&g_writer, 1);
    int status = 0;
    int count = paths_end - 1;
    for (int i = 0; i < (count ? count : 1); i++) {
        const char* root = count ? argv[1 + i] : ".";
        size_t length = strlen(root);
        if (length >= sizeof(g_path)) {
            eprintf("find: path too long\n");
            status = 1;
            continue;
        }
        memcpy(g_path, root, length + 1);
        while (length > 1 && g_path[length - 1] == '/') g_path[--length] = '\0';
        walk(length, 0);
    }
    if (wb_flush(&g_writer) < 0) status = 1;
    return status;
}
