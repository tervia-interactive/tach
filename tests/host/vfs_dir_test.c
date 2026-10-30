/* Host test: diretorios reais no VFS (readdir), stat e resolucao de caminhos. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>

/* Como nos outros testes de host, o resultado e o numero da linha que falhou
 * (0 = tudo certo). */
#define CHECK(cond) do { if (!(cond)) return __LINE__; } while (0)

static uint8_t g_arena[512 * 1024] __attribute__((aligned(16)));
static uint8_t g_pmm_arena[128 * 1024] __attribute__((aligned(PAGE_SIZE)));
static bool g_pmm_used;
void* pmm_alloc_pages(size_t count) {
    if (g_pmm_used || count * PAGE_SIZE > sizeof(g_pmm_arena)) return NULL;
    g_pmm_used = true;
    return g_pmm_arena;
}
void pmm_free_pages(void* pages, size_t count) {
    (void)count;
    if (pages == g_pmm_arena) g_pmm_used = false;
}

static const char g_payload[] = "payload";

/* Normaliza `input` e compara com `expected`. */
static int check_normalize(const char* input, const char* expected) {
    char path[64];
    strcpy(path, input);
    if (vfs_normalize_path(path) != 0) return 0;
    return strcmp(path, expected) == 0;
}

static int check_resolve(const char* cwd, const char* input,
                         const char* expected) {
    char path[64];
    if (vfs_resolve_path(cwd, input, path, sizeof(path)) != 0) return 0;
    return strcmp(path, expected) == 0;
}

/* Abre `path` e devolve a entrada `index`: 1/0/-errno como vfs_readdir. */
static int entry_at(const char* path, size_t index, char* name, int* type) {
    int error = 0;
    struct vnode* dir = vfs_open_err(path, O_RDONLY, &error);
    if (!dir) return error < 0 ? error : -ENOENT;
    struct dirent entry;
    int result = vfs_readdir(dir, index, &entry);
    if (result == 1) {
        strcpy(name, entry.d_name);
        *type = entry.d_type;
    }
    vfs_release(dir);
    return result;
}

int main(void) {
    int error = 0;
    kheap_init(g_arena, sizeof(g_arena));
    vfs_init();
    tmpfs_init();

    /* ---- normalizacao ---- */
    CHECK(check_normalize("/", "/"));
    CHECK(check_normalize("/.", "/"));
    CHECK(check_normalize("/..", "/"));
    CHECK(check_normalize("/../..", "/"));
    CHECK(check_normalize("//bin//", "/bin"));
    CHECK(check_normalize("/bin/.", "/bin"));
    CHECK(check_normalize("/bin/..", "/"));
    CHECK(check_normalize("/a/b/../../c", "/c"));
    CHECK(check_normalize("/a/b/..", "/a"));
    CHECK(check_normalize("/tmp/./x/../y", "/tmp/y"));
    CHECK(check_normalize("/...", "/..."));
    CHECK(check_normalize("/a/..b", "/a/..b"));
    char relative[8] = "bin";
    CHECK(vfs_normalize_path(relative) == -EINVAL);

    /* ---- resolucao contra o cwd ---- */
    CHECK(check_resolve("/", "bin", "/bin"));
    CHECK(check_resolve("/bin", "sh", "/bin/sh"));
    CHECK(check_resolve("/bin", "../tmp/x", "/tmp/x"));
    CHECK(check_resolve("/bin", ".", "/bin"));
    CHECK(check_resolve("/bin", "..", "/"));
    CHECK(check_resolve("/a", "/b/./c", "/b/c"));
    CHECK(check_resolve(NULL, "x", "/x"));
    CHECK(check_resolve("lixo", "x", "/x"));
    char small[5];
    CHECK(vfs_resolve_path("/", "", small, sizeof(small)) == -ENOENT);
    CHECK(vfs_resolve_path("/", "abcdefghij", small, sizeof(small)) ==
          -ENAMETOOLONG);

    /* ---- diretorios derivados dos arquivos do initrd ---- */
    CHECK(vfs_register_memfile("/bin/sh", g_payload, 7) == 0);
    CHECK(vfs_register_memfile("/bin/ls", g_payload, 7) == 0);
    CHECK(vfs_register_memfile("/sbin/init", g_payload, 7) == 0);
    CHECK(vfs_register_memfile("/etc/conf/a.txt", g_payload, 7) == 0);

    struct vnode* bin = vfs_open_err("/bin", O_RDONLY, &error);
    CHECK(bin && error == 0 && bin->type == VNODE_DIR);
    CHECK(vfs_open_err("/bin", O_RDONLY, NULL) == bin); /* mesmo vnode do pool */
    char buffer[8];
    CHECK(vfs_read(bin, buffer, sizeof(buffer), 0) == -EISDIR);
    CHECK(vfs_open_err("/bin", O_WRONLY, &error) == NULL && error == -EISDIR);
    CHECK(vfs_open_err("/bin/", O_RDONLY, &error) == NULL && error == -ENOENT);
    CHECK(vfs_open_err("/nada", O_RDONLY, &error) == NULL && error == -ENOENT);
    CHECK(vfs_open_err("/bin/nada", O_RDONLY, &error) == NULL &&
          error == -ENOENT);
    vfs_release(bin);
    vfs_release(bin);

    /* Raiz: dev, tmp (fixos), depois bin, sbin, etc (initrd), sem repeticao. */
    char name[64];
    int type = 0;
    CHECK(entry_at("/", 0, name, &type) == 1 && strcmp(name, "dev") == 0 &&
          type == VNODE_DIR);
    CHECK(entry_at("/", 1, name, &type) == 1 && strcmp(name, "tmp") == 0 &&
          type == VNODE_DIR);
    CHECK(entry_at("/", 2, name, &type) == 1 && strcmp(name, "bin") == 0 &&
          type == VNODE_DIR);
    CHECK(entry_at("/", 3, name, &type) == 1 && strcmp(name, "sbin") == 0);
    CHECK(entry_at("/", 4, name, &type) == 1 && strcmp(name, "etc") == 0);
    CHECK(entry_at("/", 5, name, &type) == 0);

    CHECK(entry_at("/bin", 0, name, &type) == 1 && strcmp(name, "sh") == 0 &&
          type == VNODE_FILE);
    CHECK(entry_at("/bin", 1, name, &type) == 1 && strcmp(name, "ls") == 0);
    CHECK(entry_at("/bin", 2, name, &type) == 0);
    CHECK(entry_at("/dev", 0, name, &type) == 1 &&
          strcmp(name, "console") == 0 && type == VNODE_CHAR);
    CHECK(entry_at("/dev", 1, name, &type) == 1 && strcmp(name, "tty") == 0);
    CHECK(entry_at("/dev", 2, name, &type) == 0);
    CHECK(entry_at("/etc", 0, name, &type) == 1 && strcmp(name, "conf") == 0 &&
          type == VNODE_DIR);
    CHECK(entry_at("/etc/conf", 0, name, &type) == 1 &&
          strcmp(name, "a.txt") == 0 && type == VNODE_FILE);

    /* Arquivo comum nao e diretorio. */
    struct vnode* file = vfs_open_err("/bin/sh", O_RDONLY, &error);
    CHECK(file && file->type == VNODE_FILE);
    struct dirent entry;
    CHECK(vfs_readdir(file, 0, &entry) == -ENOTDIR);
    vfs_release(file);

    /* ---- /tmp (tmpfs) ---- */
    CHECK(entry_at("/tmp", 0, name, &type) == 0);
    file = vfs_open_err("/tmp/z", O_WRONLY | O_CREAT, &error);
    CHECK(file && vfs_write(file, "abc", 3, 0) == 3);
    file->ops->release(file);
    CHECK(entry_at("/tmp", 0, name, &type) == 1 && strcmp(name, "z") == 0 &&
          type == VNODE_FILE);
    CHECK(entry_at("/tmp", 1, name, &type) == 0);

    /* ---- stat ---- */
    struct tach_stat info;
    CHECK(vfs_stat("/bin/sh", &info) == 0);
    CHECK(TACH_S_ISREG(info.st_mode) && (info.st_mode & 0777u) == 0755u);
    CHECK(info.st_size == 7 && info.st_nlink == 1);
    CHECK(vfs_stat("/etc/conf/a.txt", &info) == 0 &&
          (info.st_mode & 0777u) == 0644u);
    CHECK(vfs_stat("/bin", &info) == 0 && TACH_S_ISDIR(info.st_mode) &&
          info.st_size == 0);
    CHECK(vfs_stat("/", &info) == 0 && TACH_S_ISDIR(info.st_mode));
    CHECK(vfs_stat("/dev/console", &info) == 0 && TACH_S_ISCHR(info.st_mode));
    CHECK(vfs_stat("/nada", &info) == -ENOENT);
    CHECK(vfs_stat("/tmp/z", &info) == 0 && info.st_size == 3);
    /* O stat nao pode deixar o arquivo "aberto": o unlink tem de passar. */
    CHECK(tmpfs_unlink("/tmp/z") == 0);
    CHECK(entry_at("/tmp", 0, name, &type) == 0);
    CHECK(vfs_stat_vnode(NULL, &info) == -EINVAL);

    return 0;
}
