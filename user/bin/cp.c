#include <tach.h>
#include <tachc.h>

/* cp [-f] origem destino | cp [-f] origem... diretorio
 * Copia arquivos comuns. Sem -r: o kernel ainda nao expoe mkdir. */

#define PATH_MAX_LEN 256

static unsigned char g_buf[2048];
static char g_target[PATH_MAX_LEN];
static struct stat g_src_stat;
static struct stat g_dst_stat;

static int is_dir(const char* path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

/* Copia src para dst. Devolve 0, ou 1 em erro (ja reportado). */
static int copy_one(const char* src, const char* dst, int force) {
    if (stat(src, &g_src_stat) < 0) {
        eprintf("cp: %s: no such file\n", src);
        return 1;
    }
    if (S_ISDIR(g_src_stat.st_mode)) {
        eprintf("cp: %s: is a directory (omitted)\n", src);
        return 1;
    }
    if (stat(dst, &g_dst_stat) == 0) {
        if (strcmp(src, dst) == 0 ||
            (g_src_stat.st_ino == g_dst_stat.st_ino &&
             g_src_stat.st_mode == g_dst_stat.st_mode &&
             g_src_stat.st_size == g_dst_stat.st_size)) {
            eprintf("cp: %s and %s are the same file\n", src, dst);
            return 1;
        }
    }
    int in = open(src, O_RDONLY);
    if (in < 0) {
        eprintf("cp: %s: cannot open\n", src);
        return 1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0 && force) {
        (void)unlink(dst);
        out = open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    }
    if (out < 0) {
        eprintf("cp: %s: cannot create\n", dst);
        (void)close(in);
        return 1;
    }
    int status = 0;
    for (;;) {
        ssize_t count = read(in, g_buf, sizeof(g_buf));
        if (count < 0) {
            eprintf("cp: %s: read error\n", src);
            status = 1;
            break;
        }
        if (count == 0) break;
        if (write_all(out, g_buf, (size_t)count) < 0) {
            eprintf("cp: %s: write error\n", dst);
            status = 1;
            break;
        }
    }
    (void)close(in);
    (void)close(out);
    return status;
}

int main(int argc, char** argv) {
    int force = 0;
    int c;
    while ((c = getopt(argc, argv, "fr")) != -1) {
        if (c == 'f') force = 1;
        else if (c == 'r') {
            eprintf("cp: -r is not supported (no mkdir syscall yet)\n");
            return 2;
        } else {
            eprintf("usage: cp [-f] src dst | cp [-f] src... dir\n");
            return 2;
        }
    }
    int count = argc - optind;
    if (count < 2) {
        eprintf("usage: cp [-f] src dst | cp [-f] src... dir\n");
        return 2;
    }
    const char* last = argv[argc - 1];
    if (count > 2 && !is_dir(last)) {
        eprintf("cp: target '%s' is not a directory\n", last);
        return 2;
    }
    if (!is_dir(last)) return copy_one(argv[optind], last, force);

    int status = 0;
    size_t dir_length = strlen(last);
    while (dir_length > 1 && last[dir_length - 1] == '/') dir_length--;
    for (int i = optind; i < argc - 1; i++) {
        const char* name = path_basename(argv[i]);
        size_t name_length = strlen(name);
        if (!name_length || dir_length + 1 + name_length >= sizeof(g_target)) {
            eprintf("cp: %s: bad name\n", argv[i]);
            status = 1;
            continue;
        }
        memcpy(g_target, last, dir_length);
        size_t position = dir_length;
        if (!(dir_length == 1 && last[0] == '/')) g_target[position++] = '/';
        memcpy(g_target + position, name, name_length + 1);
        if (copy_one(argv[i], g_target, force)) status = 1;
    }
    return status;
}
