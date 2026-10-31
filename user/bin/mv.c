#include <tach.h>
#include <tachc.h>

/* mv origem destino | mv origem... diretorio
 * Tenta rename() primeiro (atomico, funciona para diretorios na mesma
 * montagem). Se a montagem nao suporta (EXDEV/EROFS/ENOTSUP), copia o
 * conteudo e remove a origem (so arquivos comuns, e so apaga a origem se a
 * copia terminou sem erro). */
#define MV_EXDEV 18
#define MV_EROFS 30
#define MV_ENOTSUP 103

#define PATH_MAX_LEN 256

static unsigned char g_buf[2048];
static char g_target[PATH_MAX_LEN];
static struct stat g_src_stat;
static struct stat g_dst_stat;

static int is_dir(const char* path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int move_one(const char* src, const char* dst) {
    if (stat(src, &g_src_stat) < 0) {
        eprintf("mv: %s: no such file\n", src);
        return 1;
    }
    int renamed = rename(src, dst);
    if (renamed == 0) return 0;
    if (renamed != -MV_EXDEV && renamed != -MV_EROFS && renamed != -MV_ENOTSUP) {
        eprintf("mv: cannot move %s to %s\n", src, dst);
        return 1;
    }
    if (S_ISDIR(g_src_stat.st_mode)) {
        eprintf("mv: %s: moving directories across file systems is not supported\n", src);
        return 1;
    }
    if (stat(dst, &g_dst_stat) == 0) {
        if (strcmp(src, dst) == 0 ||
            (g_src_stat.st_ino == g_dst_stat.st_ino &&
             g_src_stat.st_mode == g_dst_stat.st_mode &&
             g_src_stat.st_size == g_dst_stat.st_size)) {
            eprintf("mv: %s and %s are the same file\n", src, dst);
            return 1;
        }
    }
    int in = open(src, O_RDONLY);
    if (in < 0) {
        eprintf("mv: %s: cannot open\n", src);
        return 1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC);
    if (out < 0) {
        eprintf("mv: %s: cannot create\n", dst);
        (void)close(in);
        return 1;
    }
    int failed = 0;
    for (;;) {
        ssize_t count = read(in, g_buf, sizeof(g_buf));
        if (count < 0) { failed = 1; break; }
        if (count == 0) break;
        if (write_all(out, g_buf, (size_t)count) < 0) { failed = 1; break; }
    }
    (void)close(in);
    (void)close(out);
    if (failed) {
        eprintf("mv: %s: copy failed; source kept\n", src);
        return 1;
    }
    if (unlink(src) < 0) {
        eprintf("mv: %s: copied but cannot remove source\n", src);
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        eprintf("usage: mv src dst | mv src... dir\n");
        return 2;
    }
    const char* last = argv[argc - 1];
    int count = argc - 2;
    if (count > 1 && !is_dir(last)) {
        eprintf("mv: target '%s' is not a directory\n", last);
        return 2;
    }
    if (!is_dir(last)) return move_one(argv[1], last);

    int status = 0;
    size_t dir_length = strlen(last);
    while (dir_length > 1 && last[dir_length - 1] == '/') dir_length--;
    for (int i = 1; i < argc - 1; i++) {
        const char* name = path_basename(argv[i]);
        size_t name_length = strlen(name);
        if (!name_length || dir_length + 1 + name_length >= sizeof(g_target)) {
            eprintf("mv: %s: bad name\n", argv[i]);
            status = 1;
            continue;
        }
        memcpy(g_target, last, dir_length);
        size_t position = dir_length;
        if (!(dir_length == 1 && last[0] == '/')) g_target[position++] = '/';
        memcpy(g_target + position, name, name_length + 1);
        if (move_one(argv[i], g_target)) status = 1;
    }
    return status;
}
