/* Host test: tabela de montagem do VFS (etapa 4.1) com um FS falso.
 * NAO compilado quando escrito. Retorna __LINE__ da falha. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>

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

/* FS falso: guarda o ultimo caminho relativo recebido e o numero de abertos. */
struct fake { char last[64]; int opened; int unmounts; int syncs; };

static struct vnode g_fake_vn;
static struct vnode* f_open(void* fs, const char* rel, int flags, int* err) {
    struct fake* f = fs;
    (void)flags;
    strcpy(f->last, rel);
    if (strcmp(rel, "/missing") == 0) { *err = -ENOENT; return NULL; }
    f->opened++;
    g_fake_vn.type = VNODE_FILE;
    return &g_fake_vn;
}
static int f_unlink(void* fs, const char* rel) { strcpy(((struct fake*)fs)->last, rel); return 0; }
static int f_mkdir(void* fs, const char* rel) { strcpy(((struct fake*)fs)->last, rel); return 0; }
static int f_rename(void* fs, const char* a, const char* b) {
    (void)b;
    strcpy(((struct fake*)fs)->last, a);
    return 0;
}
static int f_sync(void* fs) { ((struct fake*)fs)->syncs++; return 0; }
static int f_unmount(void* fs) {
    struct fake* f = fs;
    if (f->opened) return -EBUSY;
    f->unmounts++;
    return 0;
}
static const struct vfs_fsops g_ops = {
    .open = f_open, .unlink = f_unlink, .mkdir = f_mkdir, .rename = f_rename,
    .sync = f_sync, .unmount = f_unmount,
};

int main(void) {
    struct fake a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    kheap_init(g_arena, sizeof(g_arena));
    vfs_init();
    tmpfs_init();

    CHECK(vfs_mount_fs("/", &g_ops, &a) == -EINVAL);
    CHECK(vfs_mount_fs("mnt", &g_ops, &a) == -EINVAL);
    CHECK(vfs_mount_fs("/mnt/", &g_ops, &a) == -EINVAL);   /* nao normalizado */
    CHECK(vfs_mount_fs("/mnt", &g_ops, &a) == 0);
    CHECK(vfs_mount_fs("/mnt", &g_ops, &b) == -EBUSY);
    CHECK(vfs_mount_fs("/mnt/sub", &g_ops, &b) == 0);

    /* maior prefixo e fronteira de componente */
    int err = 0;
    struct vnode* v = vfs_open_err("/mnt/x/y", O_RDONLY, &err);
    CHECK(v && err == 0 && strcmp(a.last, "/x/y") == 0);
    vfs_release(v);
    v = vfs_open_err("/mnt", O_RDONLY, &err);
    CHECK(v && strcmp(a.last, "/") == 0);
    vfs_release(v);
    v = vfs_open_err("/mnt/sub/z", O_RDONLY, &err);
    CHECK(v && strcmp(b.last, "/z") == 0);
    vfs_release(v);
    a.last[0] = '\0';
    v = vfs_open_err("/mntx/q", O_RDONLY, &err);
    CHECK(!v && a.last[0] == '\0');                         /* nao casa /mnt */
    v = vfs_open_err("/mnt/missing", O_RDONLY, &err);
    CHECK(!v && err == -ENOENT);

    /* operacoes de diretorio */
    CHECK(vfs_mkdir("/mnt/d") == 0 && strcmp(a.last, "/d") == 0);
    CHECK(vfs_rmdir("/mnt/d") == -ENOTSUP);
    CHECK(vfs_unlink("/mnt/f") == 0 && strcmp(a.last, "/f") == 0);
    CHECK(vfs_rename("/mnt/a", "/mnt/b") == 0);
    CHECK(vfs_rename("/mnt/a", "/mnt/sub/b") == -EXDEV);
    CHECK(vfs_rename("/mnt/a", "/tmp/b") == -EXDEV);
    CHECK(vfs_mkdir("/etc/x") == -EROFS);

    /* /tmp continua no tmpfs */
    struct vnode* t = vfs_open_err("/tmp/hello", O_CREAT | O_RDWR, &err);
    CHECK(t && err == 0);
    vfs_release(t);

    /* sync e umount */
    CHECK(vfs_sync() == 0 && a.syncs == 1 && b.syncs == 1);
    a.opened = 1;
    CHECK(vfs_umount("/mnt") == -EBUSY);
    a.opened = 0;
    CHECK(vfs_umount("/mnt") == 0 && a.unmounts == 1);
    CHECK(vfs_umount("/mnt") == -EINVAL);
    CHECK(vfs_umount("/mnt/sub") == 0);
    v = vfs_open_err("/mnt/x", O_RDONLY, &err);
    CHECK(!v);
    return 0;
}
