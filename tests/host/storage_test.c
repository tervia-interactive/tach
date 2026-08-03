#include <drivers/block/block.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <kernel/string.h>

extern void* calloc(size_t count, size_t size);
extern void free(void* pointer);

#define CHECK(expr) do { if (!(expr)) { free(g_disk); return __LINE__; } } while (0)
#define TEST_SECTORS 70000u
#define TEST_FAT_SECTORS 600u

static uint8_t* g_disk;
static unsigned g_sync_count;

void hal_console_write(const char* text, size_t length) {
    (void)text; (void)length;
}
char hal_console_getchar(void) { return '\n'; }

static int memory_read(struct block_device* device, uint64_t lba,
                       uint32_t count, void* buffer) {
    (void)device;
    if (lba + count > TEST_SECTORS) return -1;
    memcpy(buffer, g_disk + (size_t)lba * 512, (size_t)count * 512);
    return 0;
}
static int memory_write(struct block_device* device, uint64_t lba,
                        uint32_t count, const void* buffer) {
    (void)device;
    if (lba + count > TEST_SECTORS) return -1;
    memcpy(g_disk + (size_t)lba * 512, buffer, (size_t)count * 512);
    return 0;
}
static int memory_sync(struct block_device* device) {
    (void)device;
    g_sync_count++;
    return 0;
}
static const struct block_device_ops g_ops = {
    .read = memory_read, .write = memory_write, .sync = memory_sync
};

static void format_test_volume(void) {
    struct fat32_boot_sector* boot = (struct fat32_boot_sector*)g_disk;
    memset(boot, 0, 512);
    boot->jmp[0] = 0xeb; boot->jmp[1] = 0x58; boot->jmp[2] = 0x90;
    memcpy(boot->oem_name, "tachtest", 8);
    boot->bytes_per_sector = 512; boot->sectors_per_cluster = 1;
    boot->reserved_sectors = 32; boot->num_fats = 2;
    boot->media_type = 0xf8; boot->total_sectors_32 = TEST_SECTORS;
    boot->fat_size_32 = TEST_FAT_SECTORS; boot->root_cluster = 2;
    g_disk[510] = 0x55; g_disk[511] = 0xaa;
    for (uint32_t fat = 0; fat < 2; fat++) {
        uint32_t* table = (uint32_t*)(g_disk +
            (size_t)(32 + fat * TEST_FAT_SECTORS) * 512);
        table[0] = 0x0ffffff8u; table[1] = 0xffffffffu;
        table[2] = 0x0fffffffu;
    }
}

int main(void) {
    g_disk = calloc(TEST_SECTORS, 512);
    CHECK(g_disk != NULL);
    format_test_volume();
    struct block_device device = {
        .name = "mem0", .sector_size = 512, .sector_count = TEST_SECTORS,
        .ops = &g_ops
    };
    vfs_init();
    struct vnode* root;
    CHECK(fat32_mount(&device, &root) == 0);
    CHECK(vfs_mount("/disk", root) == 0);
    CHECK(vfs_mkdir("/disk/docs") == 0);
    struct vnode* file = vfs_open("/disk/docs/hello.txt",
                                  O_CREAT | O_WRONLY | O_TRUNC);
    CHECK(file != NULL);
    const char message[] = "persistent across remount\n";
    CHECK(vfs_write(file, message, sizeof(message) - 1, 0) ==
          (ssize_t)(sizeof(message) - 1));
    CHECK(vfs_close(file) == 0);
    CHECK(vfs_sync() == 0);
    CHECK(g_sync_count == 1);

    vfs_init();
    CHECK(fat32_mount(&device, &root) == 0);
    CHECK(vfs_mount("/disk", root) == 0);
    file = vfs_open("/disk/docs/hello.txt", O_RDONLY);
    CHECK(file != NULL);
    char output[64]; memset(output, 0, sizeof(output));
    CHECK(vfs_read(file, output, sizeof(output), 0) ==
          (ssize_t)(sizeof(message) - 1));
    CHECK(memcmp(output, message, sizeof(message)) == 0);
    struct vnode* directory = vfs_open("/disk/docs", O_RDONLY);
    CHECK(directory != NULL);
    struct dirent entry;
    CHECK(vfs_readdir(directory, &entry, 0) == 1);
    CHECK(!strcmp(entry.d_name, "."));
    CHECK(vfs_readdir(directory, &entry, 2) == 1);
    CHECK(!strcmp(entry.d_name, "hello.txt"));
    CHECK(vfs_unlink("/disk/docs/hello.txt") == 0);
    CHECK(vfs_unlink("/disk/docs") == 0);
    free(g_disk);
    return 0;
}
