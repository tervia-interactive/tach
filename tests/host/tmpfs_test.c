/* Host test: /tmp (tmpfs) create/write/append/list/unlink semantics. */
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

int main(void) {
    int error = 0;
    kheap_init(g_arena, sizeof(g_arena));
    vfs_init();
    tmpfs_init();

    CHECK(tmpfs_owns("/tmp") && tmpfs_owns("/tmp/a") && !tmpfs_owns("/tmpx"));
    CHECK(vfs_open_err("/tmp/a", O_RDONLY, &error) == NULL && error == -ENOENT);

    struct vnode* file = vfs_open_err("/tmp/a", O_WRONLY | O_CREAT, &error);
    CHECK(file && error == 0);
    CHECK(vfs_write(file, "hello", 5, 0) == 5);
    CHECK(vfs_write(file, " world", 6, 5) == 6);
    CHECK(file->size == 11);
    char buffer[32];
    memset(buffer, 0, sizeof(buffer));
    CHECK(vfs_read(file, buffer, sizeof(buffer), 0) == 11);
    CHECK(memcmp(buffer, "hello world", 11) == 0);
    CHECK(vfs_read(file, buffer, 4, 11) == 0);

    /* Escrita alem do fim: o buraco le como zeros. */
    CHECK(vfs_write(file, "!", 1, 14) == 1 && file->size == 15);
    CHECK(vfs_read(file, buffer, 4, 11) == 4);
    CHECK(buffer[0] == 0 && buffer[2] == 0);
    CHECK(vfs_write(file, "x", 1, (int64_t)TMPFS_MAX_FILE_SIZE) == -EFBIG);

    /* Arquivo aberto (hook open conta 1): unlink recusa; release libera. */
    CHECK(tmpfs_unlink("/tmp/a") == -EBUSY);
    file->ops->release(file);
    CHECK(tmpfs_unlink("/tmp/a") == 0);
    CHECK(tmpfs_unlink("/tmp/a") == -ENOENT);

    /* Truncar, nomes invalidos, diretorio, limite de arquivos. */
    file = vfs_open_err("/tmp/b", O_WRONLY | O_CREAT, &error);
    CHECK(file && vfs_write(file, "abc", 3, 0) == 3);
    file->ops->release(file);
    file = vfs_open_err("/tmp/b", O_WRONLY | O_TRUNC, &error);
    CHECK(file && file->size == 0);
    file->ops->release(file);
    CHECK(vfs_open_err("/tmp/x/y", O_CREAT | O_WRONLY, &error) == NULL &&
          error == -ENOTDIR);
    CHECK(vfs_open_err("/tmp/", O_RDONLY, &error) == NULL && error == -EISDIR);
    CHECK(vfs_open_err("/tmp", O_WRONLY, &error) == NULL && error == -EISDIR);

    struct vnode* dir = vfs_open_err("/tmp", O_RDONLY, &error);
    CHECK(dir && dir->type == VNODE_DIR);
    memset(buffer, 0, sizeof(buffer));
    CHECK(vfs_read(dir, buffer, sizeof(buffer), 0) == 2 &&
          strcmp(buffer, "b\n") == 0);

    char name[16] = "/tmp/f00";
    int created = 1; /* "b" ja existe */
    for (int i = 0; i < TMPFS_MAX_FILES + 2; i++) {
        name[6] = (char)('0' + i / 10);
        name[7] = (char)('0' + i % 10);
        struct vnode* made = vfs_open_err(name, O_WRONLY | O_CREAT, &error);
        if (made) { created++; made->ops->release(made); }
        else CHECK(error == -ENOSPC);
    }
    CHECK(created == TMPFS_MAX_FILES);
    CHECK(vfs_unlink("/proc/version") == -EROFS);

    return 0;
}
