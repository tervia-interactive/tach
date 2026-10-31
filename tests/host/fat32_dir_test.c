/* Host test: operacoes de diretorio FAT32 (etapa 3.2) - create/unlink/mkdir/
 * rmdir/rename, nomes 8.3 + LFN, diretorios que crescem. NAO FOI COMPILADO
 * nem executado quando escrito (a sessao proibiu compilar): rodar
 * `make test-host` e corrigir o que aparecer. Retorna __LINE__ da falha. */
#pragma GCC diagnostic ignored "-Wunused-function"
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <drivers/block/blk.h>
#include <fs/fat32.h>

ssize_t write(int fd, const void* buf, size_t count);

#define CHECK(cond) do { if (!(cond)) return __LINE__; } while (0)
#define TRY(expr) do { int try_result = (expr); if (try_result) return try_result; } while (0)

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

static void say(const char* text) { (void)write(2, text, strlen(text)); }
static void say_number(unsigned value) {
    char digits[12];
    size_t n = sizeof(digits);
    digits[--n] = '\n';
    do { digits[--n] = (char)('0' + value % 10u); value /= 10u; } while (value);
    (void)write(2, digits + n, sizeof(digits) - n);
}

/* ============================================================ formatador */

#define IMG_BYTES   (72u << 20)
#define CLUSTERS    65600u
#define EOC         0x0FFFFFFFu
#define RESERVED    32u

static uint8_t g_img[IMG_BYTES] __attribute__((aligned(16)));
static uint8_t g_fat0_snapshot[600u * 512u];
static uint8_t g_seen[CLUSTERS + 2u];
static struct { uint32_t spc, fat_sectors, total, data_start, next; } g;

static void w16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* p, uint32_t v) { w16(p, v); w16(p + 2, v >> 16); }
static uint32_t r16(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t r32(const uint8_t* p) { return r16(p) | r16(p + 2) << 16; }

static uint8_t* fat_ptr(uint32_t copy, uint32_t cluster) {
    return g_img + ((size_t)RESERVED + (size_t)copy * g.fat_sectors) * 512u +
           (size_t)cluster * 4u;
}
static void fat_put(uint32_t cluster, uint32_t value) {
    w32(fat_ptr(0, cluster), value);
    w32(fat_ptr(1, cluster), value);
}
static uint32_t fat_get(uint32_t copy, uint32_t cluster) {
    return r32(fat_ptr(copy, cluster)) & 0x0FFFFFFFu;
}
static uint8_t* cluster_ptr(uint32_t cluster) {
    return g_img + ((size_t)g.data_start + (size_t)(cluster - 2u) * g.spc) * 512u;
}

static void format(uint32_t spc, uint32_t ext_flags) {
    g.spc = spc;
    g.fat_sectors = (CLUSTERS + 2u + 127u) / 128u;
    g.data_start = RESERVED + 2u * g.fat_sectors;
    g.total = g.data_start + CLUSTERS * spc;
    memset(g_img, 0, (size_t)g.data_start * 512u);
    memset(g_img + (size_t)g.data_start * 512u, 0xEE,
           (size_t)CLUSTERS * spc * 512u);          /* "lixo" nos livres */
    uint8_t* b = g_img;
    b[0] = 0xEB; b[1] = 0x58; b[2] = 0x90;
    memcpy(b + 3, "TACHTEST", 8);
    w16(b + 11, 512);
    b[13] = (uint8_t)spc;
    w16(b + 14, RESERVED);
    b[16] = 2;
    b[21] = 0xF8;
    w16(b + 24, 63);
    w16(b + 26, 255);
    w32(b + 32, g.total);
    w32(b + 36, g.fat_sectors);
    w16(b + 40, ext_flags);
    w32(b + 44, 2);
    w16(b + 48, 1);                                 /* setor do FSInfo */
    w16(b + 50, 6);
    b[66] = 0x29;
    memcpy(b + 71, "TACHVOL    ", 11);
    memcpy(b + 82, "FAT32   ", 8);
    b[510] = 0x55; b[511] = 0xAA;
    fat_put(0, 0x0FFFFFF8u);
    fat_put(1, EOC);
    fat_put(2, EOC);                                /* raiz: um cluster */
    g.next = 3;
}

static uint32_t take_cluster(void) {
    uint32_t cluster = g.next++;
    fat_put(cluster, EOC);
    return cluster;
}

static uint8_t pat(uint32_t seed, uint32_t i) {
    return (uint8_t)(i * 131u + (i >> 8) * 17u + seed * 7u + 3u);
}

/* Cadeia com `size` bytes de padrao. mode 1: fragmentada (pula clusters). */
static uint32_t make_chain(uint32_t size, uint32_t seed, int mode) {
    if (!size) return 0;
    uint32_t csize = g.spc * 512u;
    uint32_t n = (size + csize - 1u) / csize;
    uint32_t list[16];
    for (uint32_t i = 0; i < n && i < 16u; i++) {
        list[i] = g.next;
        g.next += mode == 1 ? 3u : 1u;
    }
    for (uint32_t i = 0; i < n; i++) {
        fat_put(list[i], i + 1u < n ? list[i + 1u] : EOC);
        uint32_t bytes = size - i * csize < csize ? size - i * csize : csize;
        for (uint32_t j = 0; j < bytes; j++)
            cluster_ptr(list[i])[j] = pat(seed, i * csize + j);
    }
    return list[0];
}

static uint32_t g_root_used;

static void put_root(const char* name11, uint8_t attr, uint32_t cluster,
                     uint32_t size) {
    uint8_t* e = cluster_ptr(2) + g_root_used * 32u;
    memset(e, 0, 32);
    memcpy(e, name11, 11);
    e[11] = attr;
    w16(e + 22, 0x6000);
    w16(e + 24, 0x5A21);
    w16(e + 20, cluster >> 16);
    w16(e + 26, cluster & 0xFFFFu);
    w32(e + 28, size);
    g_root_used++;
}

static uint32_t free_in_image(void) {
    uint32_t n = 0;
    for (uint32_t c = 2; c < CLUSTERS + 2u; c++)
        if (fat_get(0, c) == 0) n++;
    return n;
}

#define SIZE_C_EXTRA 100u               /* C.BIN = 5 clusters + 100 bytes */

static uint32_t csize(void) { return g.spc * 512u; }

/* Imagem padrao: A.BIN vazio, B.BIN 1000 B, C.BIN fragmentado, RO.TXT. */
static void populate(void) {
    memset(cluster_ptr(2), 0, g.spc * 512u);        /* raiz zerada */
    g_root_used = 0;
    put_root("A       BIN", 0x20, 0, 0);
    put_root("B       BIN", 0x20, make_chain(1000, 5, 0), 1000);
    put_root("C       BIN", 0x20, make_chain(5u * csize() + SIZE_C_EXTRA, 6, 1),
             5u * csize() + SIZE_C_EXTRA);
    put_root("RO      TXT", 0x21, make_chain(10, 7, 0), 10);
    uint8_t* f = g_img + 512u;                      /* FSInfo */
    memset(f, 0, 512);
    w32(f, 0x41615252u);
    w32(f + 484, 0x61417272u);
    w32(f + 488, free_in_image());   /* cadeias fragmentadas deixam buracos */
    w32(f + 492, g.next);
    w32(f + 508, 0xAA550000u);
}

/* =================================================== montagem / invariantes */

static struct blkdev* g_dev;
static struct fat32_vol* g_vol;

static int open_vol(bool readonly) {
    g_dev = ramdisk_create("ram0", g_img, (size_t)g.total * 512u, readonly);
    CHECK(g_dev);
    CHECK(fat32_mount(g_dev, &g_vol) == 0 && g_vol);
    return 0;
}

static int close_vol(void) {
    CHECK(fat32_unmount(g_vol) == 0);
    g_vol = NULL;
    ramdisk_destroy(g_dev);                         /* sincroniza o cache */
    g_dev = NULL;
    return 0;
}

static int fresh(uint32_t spc) {
    format(spc, 0);
    populate();
    return open_vol(false);
}

static bool same(const char* a, const char* b) {
    size_t n = strlen(a);
    return n == strlen(b) && memcmp(a, b, n) == 0;
}

static void num_name(char* out, const char* pre, unsigned n) {
    size_t l = strlen(pre);
    char d[12];
    int k = 0;
    memcpy(out, pre, l);
    do { d[k++] = (char)('0' + n % 10u); n /= 10u; } while (n);
    while (k) out[l++] = d[--k];
    memcpy(out + l, ".dat", 5);
}

/* FAT1 == FAT2 e FSInfo.free_count == varredura (volume desmontado). */
static int check_fat(void) {
    for (uint32_t c = 2; c < CLUSTERS + 2u; c++)
        CHECK(fat_get(0, c) == fat_get(1, c));
    CHECK(r32(g_img + 512u + 488) == free_in_image());
    return 0;
}

static int reopen(void) {
    TRY(close_vol());
    TRY(check_fat());
    return open_vol(false);
}

static int count_dir(const char* path) {
    struct fat32_info info;
    struct fat32_node node;
    CHECK(fat32_lookup(g_vol, path, &info) == 0);
    fat32_node_init(&node, &info);
    int n = 0;
    struct fat32_info e;
    while (fat32_readdir(g_vol, &node, (size_t)n, &e) == 1) n++;
    return n;
}

static uint32_t free_now(void) {
    uint32_t f = 0;
    return fat32_free_clusters(g_vol, &f) == 0 ? f : 0xFFFFFFFFu;
}

static int test_create_names(uint32_t spc) {
    struct fat32_info in;
    TRY(fresh(spc));
    CHECK(fat32_create(g_vol, "/new.txt", &in) == 0);
    CHECK(same(in.name, "new.txt") && same(in.short_name, "new.txt"));
    CHECK(fat32_create(g_vol, "/Long File Name.txt", &in) == 0);
    CHECK(same(in.name, "Long File Name.txt") && same(in.short_name, "LONGFI~1.TXT"));
    CHECK(fat32_create(g_vol, "/Long File Name 2.txt", &in) == 0);
    CHECK(same(in.short_name, "LONGFI~2.TXT"));
    CHECK(fat32_create(g_vol, "/Mixed.Case", &in) == 0);
    CHECK(same(in.name, "Mixed.Case") && same(in.short_name, "MIXED~1.CAS"));
    CHECK(fat32_create(g_vol, "/caf\xC3\xA9.txt", &in) == 0);
    CHECK(fat32_create(g_vol, "/NEW.TXT", NULL) == -EEXIST);
    CHECK(fat32_create(g_vol, "/long file name.TXT", NULL) == -EEXIST);
    CHECK(fat32_create(g_vol, "/a:b", NULL) == -EINVAL);
    CHECK(fat32_create(g_vol, "/trailing.", NULL) == -EINVAL);
    CHECK(fat32_create(g_vol, "/x/y", NULL) == -ENOENT);
    CHECK(fat32_create(g_vol, "/B.BIN/y", NULL) == -ENOTDIR);
    TRY(reopen());
    CHECK(fat32_lookup(g_vol, "/Long File Name.txt", &in) == 0 && in.size == 0 &&
          in.first_cluster == 0);
    CHECK(fat32_lookup(g_vol, "/LONGFI~2.TXT", &in) == 0 &&
          same(in.name, "Long File Name 2.txt"));
    CHECK(fat32_lookup(g_vol, "/caf\xC3\xA9.txt", &in) == 0);
    CHECK(fat32_lookup(g_vol, "/new.txt", &in) == 0);
    CHECK(count_dir("/") == 4 + 5);
    return close_vol();
}

static int test_write_new(uint32_t spc) {
    struct fat32_info in;
    struct fat32_node node;
    uint8_t buf[1500], back[1500];
    TRY(fresh(spc));
    for (unsigned i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(i * 7u + 3u);
    CHECK(fat32_create(g_vol, "/w.bin", &in) == 0);
    fat32_node_init(&node, &in);
    CHECK(fat32_write(g_vol, &node, 0, buf, sizeof(buf)) == (ssize_t)sizeof(buf));
    CHECK(fat32_node_sync(g_vol, &node) == 0);
    TRY(reopen());
    CHECK(fat32_lookup(g_vol, "/w.bin", &in) == 0 && in.size == sizeof(buf));
    fat32_node_init(&node, &in);
    CHECK(fat32_read(g_vol, &node, 0, back, sizeof(back)) == (ssize_t)sizeof(back));
    CHECK(memcmp(buf, back, sizeof(buf)) == 0);
    return close_vol();
}

static int test_dirs(uint32_t spc) {
    struct fat32_info d, e, f;
    TRY(fresh(spc));
    uint32_t free0 = free_now();
    CHECK(fat32_mkdir(g_vol, "/d") == 0);
    CHECK(fat32_mkdir(g_vol, "/d/e") == 0);
    CHECK(fat32_create(g_vol, "/d/f.txt", NULL) == 0);
    CHECK(fat32_mkdir(g_vol, "/d") == -EEXIST);
    CHECK(fat32_lookup(g_vol, "/d", &d) == 0 && (d.attr & FAT32_ATTR_DIR));
    CHECK(fat32_lookup(g_vol, "/d/e", &e) == 0);
    /* "." e ".." nos bytes da imagem (pai = raiz => ".." = 0) */
    uint8_t* p = cluster_ptr(d.first_cluster);
    CHECK(p[0] == '.' && p[32] == '.' && p[33] == '.');
    CHECK((r16(p + 20) << 16 | r16(p + 26)) == d.first_cluster);
    CHECK((r16(p + 52) << 16 | r16(p + 58)) == 0);
    p = cluster_ptr(e.first_cluster);
    CHECK((r16(p + 52) << 16 | r16(p + 58)) == d.first_cluster);
    CHECK(fat32_rmdir(g_vol, "/d") == -ENOTEMPTY);
    CHECK(fat32_rmdir(g_vol, "/d/f.txt") == -ENOTDIR);
    CHECK(fat32_unlink(g_vol, "/d") == -EISDIR);
    CHECK(fat32_unlink(g_vol, "/d/f.txt") == 0);
    CHECK(fat32_rmdir(g_vol, "/d/e") == 0);
    TRY(reopen());
    CHECK(fat32_lookup(g_vol, "/d/f.txt", &f) == -ENOENT);
    CHECK(fat32_rmdir(g_vol, "/d") == 0);
    CHECK(fat32_lookup(g_vol, "/d", &d) == -ENOENT);
    CHECK(fat32_rmdir(g_vol, "/") == -EINVAL);
    CHECK(free_now() == free0);
    return close_vol();
}

static int test_unlink(uint32_t spc) {
    struct fat32_info in;
    TRY(fresh(spc));
    uint32_t free0 = free_now();
    CHECK(fat32_unlink(g_vol, "/B.BIN") == 0);
    CHECK(free_now() == free0 + 1u);
    CHECK(fat32_unlink(g_vol, "/B.BIN") == -ENOENT);
    CHECK(fat32_unlink(g_vol, "/RO.TXT") == -EACCES);
    TRY(reopen());
    CHECK(fat32_lookup(g_vol, "/B.BIN", &in) == -ENOENT);
    CHECK(fat32_lookup(g_vol, "/C.BIN", &in) == 0);
    return close_vol();
}

static int test_many_files(uint32_t spc) {
    char name[40];
    struct fat32_info in;
    TRY(fresh(spc));
    for (unsigned i = 0; i < 300; i++) {
        num_name(name, "/f", i);
        CHECK(fat32_create(g_vol, name, NULL) == 0);
    }
    TRY(reopen());
    CHECK(count_dir("/") == 4 + 300);
    for (unsigned i = 0; i < 300; i++) {
        num_name(name, "/f", i);
        CHECK(fat32_lookup(g_vol, name, &in) == 0);
    }
    for (unsigned i = 1; i < 300; i += 2) {
        num_name(name, "/f", i);
        CHECK(fat32_unlink(g_vol, name) == 0);
    }
    TRY(reopen());
    CHECK(count_dir("/") == 4 + 150);
    for (unsigned i = 0; i < 300; i++) {
        num_name(name, "/f", i);
        CHECK(fat32_lookup(g_vol, name, &in) == ((i & 1u) ? -ENOENT : 0));
    }
    for (unsigned i = 0; i < 150; i++) {                 /* reaproveita livres */
        num_name(name, "/Longer name ", i);
        
        CHECK(fat32_create(g_vol, name, NULL) == 0);
    }
    TRY(reopen());
    CHECK(count_dir("/") == 4 + 300);
    return close_vol();
}

static int test_rename(uint32_t spc) {
    struct fat32_info in, a, b;
    TRY(fresh(spc));
    CHECK(fat32_rename(g_vol, "/B.BIN", "/Renamed Long.bin") == 0);
    CHECK(fat32_lookup(g_vol, "/Renamed Long.bin", &in) == 0 && in.size == 1000);
    CHECK(fat32_lookup(g_vol, "/B.BIN", &in) == -ENOENT);
    CHECK(fat32_mkdir(g_vol, "/d") == 0 && fat32_mkdir(g_vol, "/e") == 0);
    CHECK(fat32_rename(g_vol, "/Renamed Long.bin", "/d/moved.bin") == 0);
    CHECK(fat32_lookup(g_vol, "/d/moved.bin", &in) == 0 && in.size == 1000);
    CHECK(fat32_rename(g_vol, "/d", "/e/d2") == 0);
    CHECK(fat32_lookup(g_vol, "/e/d2/moved.bin", &in) == 0);
    CHECK(fat32_lookup(g_vol, "/e/d2", &a) == 0 && fat32_lookup(g_vol, "/e", &b) == 0);
    uint8_t* p = cluster_ptr(a.first_cluster);
    CHECK((r16(p + 52) << 16 | r16(p + 58)) == b.first_cluster);
    CHECK(fat32_rename(g_vol, "/e", "/e/d2/x") == -EINVAL);
    CHECK(fat32_rename(g_vol, "/e", "/e") == 0);
    CHECK(fat32_rename(g_vol, "/e", "/RO.TXT") == -ENOTDIR);
    CHECK(fat32_rename(g_vol, "/RO.TXT", "/e") == -EISDIR);
    uint32_t free0 = free_now();
    CHECK(fat32_rename(g_vol, "/A.BIN", "/C.BIN") == 0);   /* substitui */
    CHECK(free_now() > free0);
    CHECK(fat32_lookup(g_vol, "/C.BIN", &in) == 0 && in.size == 0);
    CHECK(fat32_lookup(g_vol, "/A.BIN", &in) == -ENOENT);
    CHECK(fat32_rename(g_vol, "/RO.TXT", "/zz") == 0);
    CHECK(fat32_rename(g_vol, "/nope", "/x") == -ENOENT);
    TRY(reopen());
    CHECK(fat32_lookup(g_vol, "/e/d2/moved.bin", &in) == 0 && in.size == 1000);
    CHECK(fat32_lookup(g_vol, "/zz", &in) == 0);
    return close_vol();
}

static int test_readonly(uint32_t spc) {
    format(spc, 0);
    populate();
    TRY(open_vol(true));
    CHECK(fat32_create(g_vol, "/x", NULL) == -EROFS);
    CHECK(fat32_mkdir(g_vol, "/x") == -EROFS);
    CHECK(fat32_unlink(g_vol, "/B.BIN") == -EROFS);
    CHECK(fat32_rename(g_vol, "/B.BIN", "/y") == -EROFS);
    return close_vol();
}

static int run_config(uint32_t spc) {
    TRY(test_create_names(spc));
    TRY(test_write_new(spc));
    TRY(test_dirs(spc));
    TRY(test_unlink(spc));
    TRY(test_many_files(spc));
    TRY(test_rename(spc));
    TRY(test_readonly(spc));
    return 0;
}

int main(void) {
    kheap_init(g_arena, sizeof(g_arena));
    blk_init();
    int r = run_config(1);
    if (!r) r = run_config(2);
    if (r) {
        say("fat32_dir_test: falhou na linha ");
        say_number((unsigned)r);
    }
    return r ? 1 : 0;
}
