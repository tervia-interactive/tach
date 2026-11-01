/* Host test: /dev/<disco> no VFS (entradas ligadas ao registro do block layer).
 * Retorna __LINE__ da falha. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <drivers/block/blk.h>
#include <fs/vfs.h>

#define CHECK(cond) do { if (!(cond)) return __LINE__; } while (0)

static uint8_t g_arena[1024 * 1024] __attribute__((aligned(16)));
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

static int test(void) {
    struct tach_stat info;
    char buf[16];
    struct blkdev* disk = ramdisk_alloc("hda", 64 * 512);
    CHECK(disk);

    CHECK(vfs_stat("/dev/hda", &info) == 0 && (info.st_mode & TACH_S_IFMT) == TACH_S_IFBLK);
    CHECK(info.st_size == 64 * 512);
    CHECK(vfs_stat("/dev/hdz", &info) == -ENOENT);

    struct vnode* node = vfs_open("/dev/hda", O_RDWR);
    CHECK(node && node->ops->write(node, "tach!", 5, 510) == 5);
    memset(buf, 0, sizeof(buf));
    CHECK(node->ops->read(node, buf, 5, 510) == 5 && memcmp(buf, "tach!", 5) == 0);
    /* leitura no fim e truncada; alem do fim devolve EOF */
    CHECK(node->ops->read(node, buf, 16, 64 * 512 - 4) == 4);
    CHECK(node->ops->read(node, buf, 16, 64 * 512) == 0);
    vfs_release(node);

    ramdisk_destroy(disk);
    CHECK(vfs_stat("/dev/hda", &info) == -ENOENT);
    return 0;
}

int main(void) {
    kheap_init(g_arena, sizeof(g_arena));
    blk_init();
    vfs_init();
    return test();
}
