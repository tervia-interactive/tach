/* Host test: escrita FAT32 (etapa 3.1) - alocacao de clusters, fat32_write,
 * fat32_truncate, FSInfo e gravacao da entrada de diretorio.
 *
 * Cada cenario formata uma imagem NOVA (area de dados cheia de 0xEE, para
 * provar que nada de lixo antigo vaza), faz as operacoes, desmonta, REMONTA
 * do zero e compara o conteudo com um modelo em memoria. Depois confere os
 * invariantes direto nos bytes da imagem: FAT1 == FAT2, cadeias sem ciclo e
 * sem cluster compartilhado, nenhum cluster orfao, tamanho de cadeia ==
 * ceil(tamanho / cluster) e FSInfo.free_count == varredura da FAT.
 *
 * Roda com dois tamanhos de cluster (512 B e 1 KiB). Retorna __LINE__ da
 * primeira checagem que falhar. */
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

/* Anda pela cadeia marcando clusters; devolve o tamanho ou 0xFFFFFFFF. */
static uint32_t walk_chain(uint32_t first) {
    uint32_t n = 0, c = first;
    for (;;) {
        if (c < 2u || c >= CLUSTERS + 2u || g_seen[c]) return 0xFFFFFFFFu;
        g_seen[c] = 1;
        n++;
        uint32_t v = fat_get(0, c);
        if (v >= 0x0FFFFFF8u) return n;
        c = v;
        if (n > CLUSTERS) return 0xFFFFFFFFu;
    }
}

/* Invariantes sobre os bytes da imagem (volume desmontado). */
static int check_image(bool mirrored) {
    if (mirrored)
        CHECK(memcmp(fat_ptr(0, 0), fat_ptr(1, 0), (size_t)g.fat_sectors * 512u) == 0);
    memset(g_seen, 0, sizeof(g_seen));
    CHECK(walk_chain(2) == 1u);                     /* raiz */
    for (uint32_t pos = 0; pos < g.spc * 16u; pos++) {
        const uint8_t* e = cluster_ptr(2) + pos * 32u;
        if (e[0] == 0x00) break;
        if (e[0] == 0xE5 || e[11] == 0x0F) continue;
        uint32_t first = r16(e + 20) << 16 | r16(e + 26);
        uint32_t size = r32(e + 28);
        if (first == 0) {
            CHECK(size == 0);
            continue;
        }
        uint32_t len = walk_chain(first);
        CHECK(len != 0xFFFFFFFFu);
        CHECK(len == (size + csize() - 1u) / csize());
    }
    uint32_t free_clusters = 0;
    for (uint32_t c = 2; c < CLUSTERS + 2u; c++) {
        if (fat_get(0, c) == 0) free_clusters++;
        else CHECK(g_seen[c]);                      /* sem orfaos */
    }
    CHECK(r32(g_img + 512u + 488) == free_clusters);
    return 0;
}

/* ========================================================== modelo / leitura */

static uint8_t g_model[3u << 20];
static uint8_t g_rbuf[3u << 20];

static int open_node(const char* path, struct fat32_node* node) {
    struct fat32_info info;
    CHECK(fat32_lookup(g_vol, path, &info) == 0);
    fat32_node_init(node, &info);
    return 0;
}

static int file_equals(const char* path, uint32_t size) {
    struct fat32_info info;
    CHECK(fat32_lookup(g_vol, path, &info) == 0);
    CHECK(info.size == size);
    struct fat32_node node;
    fat32_node_init(&node, &info);
    uint32_t got = 0;
    while (got < size) {
        ssize_t n = fat32_read(g_vol, &node, got, g_rbuf + got, size - got);
        CHECK(n > 0);
        got += (uint32_t)n;
    }
    CHECK(memcmp(g_rbuf, g_model, size) == 0);
    return 0;
}

static void model_pattern(uint32_t seed, uint32_t from, uint32_t to) {
    for (uint32_t i = from; i < to; i++) g_model[i] = pat(seed, i);
}

/* ================================================================ cenarios */

/* Escreve N bytes num arquivo vazio, de uma vez. */
static int test_write_sizes(uint32_t spc) {
    uint32_t cs = spc * 512u;
    uint32_t sizes[] = {1, 511, 512, 513, cs - 1u, cs, cs + 1u, 2u << 20};
    for (unsigned k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
        uint32_t n = sizes[k];
        TRY(fresh(spc));
        uint32_t free_before = 0;
        CHECK(fat32_free_clusters(g_vol, &free_before) == 0);
        struct fat32_node node;
        TRY(open_node("/A.BIN", &node));
        model_pattern(9, 0, n);
        CHECK(fat32_write(g_vol, &node, 0, g_model, n) == (ssize_t)n);
        CHECK(node.info.size == n && node.info.first_cluster >= 2u && node.dirty);
        uint32_t free_after = 0;
        CHECK(fat32_free_clusters(g_vol, &free_after) == 0);
        CHECK(free_before - free_after == (n + cs - 1u) / cs);
        CHECK(fat32_node_sync(g_vol, &node) == 0 && !node.dirty);
        TRY(close_vol());
        TRY(check_image(true));
        TRY(open_vol(false));                       /* remonta do zero */
        TRY(file_equals("/A.BIN", n));
        TRY(close_vol());
    }
    return 0;
}

/* Sobrescreve no meio (cruzando setor), estende e escreve alem do EOF. */
static int test_overwrite_and_hole(uint32_t spc) {
    uint32_t cs = spc * 512u;
    TRY(fresh(spc));
    struct fat32_node node;
    TRY(open_node("/B.BIN", &node));
    model_pattern(5, 0, 1000);                      /* conteudo original */
    static uint8_t patch[100];
    for (uint32_t i = 0; i < 100u; i++) patch[i] = (uint8_t)(0xA0u + i);
    CHECK(fat32_write(g_vol, &node, 450, patch, 100) == 100);
    memcpy(g_model + 450, patch, 100);
    CHECK(node.info.size == 1000);

    uint32_t far = 3u * cs + 17u;                   /* buraco de zeros */
    static const uint8_t tail[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CHECK(fat32_write(g_vol, &node, far, tail, 10) == 10);
    memset(g_model + 1000, 0, far - 1000u);
    memcpy(g_model + far, tail, 10);
    CHECK(node.info.size == far + 10u);
    CHECK(fat32_node_sync(g_vol, &node) == 0);
    TRY(close_vol());
    TRY(check_image(true));
    TRY(open_vol(false));
    TRY(file_equals("/B.BIN", far + 10u));
    TRY(close_vol());
    return 0;
}

/* Escritas pequenas em sequencia (como o syscall write faz, 256 B). */
static int test_append_chunks(uint32_t spc) {
    TRY(fresh(spc));
    struct fat32_node node;
    TRY(open_node("/A.BIN", &node));
    uint32_t total = 40000;
    model_pattern(3, 0, total);
    for (uint32_t off = 0; off < total; off += 256u) {
        uint32_t n = total - off < 256u ? total - off : 256u;
        CHECK(fat32_write(g_vol, &node, off, g_model + off, n) == (ssize_t)n);
    }
    CHECK(fat32_node_sync(g_vol, &node) == 0);
    TRY(close_vol());
    TRY(check_image(true));
    TRY(open_vol(false));
    TRY(file_equals("/A.BIN", total));
    TRY(close_vol());
    return 0;
}

static int test_truncate(uint32_t spc) {
    uint32_t cs = spc * 512u;
    uint32_t full = 5u * cs + SIZE_C_EXTRA;
    uint32_t free0 = 0;

    /* encolher para varios tamanhos e conferir clusters liberados */
    uint32_t targets[] = {full - 1u, 3u * cs, 3u * cs + 1u, cs, 1, 0};
    for (unsigned k = 0; k < sizeof(targets) / sizeof(targets[0]); k++) {
        TRY(fresh(spc));
        struct fat32_node node;
        TRY(open_node("/C.BIN", &node));
        CHECK(fat32_free_clusters(g_vol, &free0) == 0);
        uint32_t t = targets[k];
        CHECK(fat32_truncate(g_vol, &node, t) == 0);
        CHECK(node.info.size == t);
        if (t == 0) CHECK(node.info.first_cluster == 0);
        uint32_t keep = (t + cs - 1u) / cs, had = 6u;
        uint32_t free1 = 0;
        CHECK(fat32_free_clusters(g_vol, &free1) == 0);
        CHECK(free1 - free0 == had - keep);
        TRY(close_vol());
        TRY(check_image(true));
        TRY(open_vol(false));
        model_pattern(6, 0, t);
        TRY(file_equals("/C.BIN", t));
        TRY(close_vol());
    }

    /* encolher e crescer: a cauda velha NAO pode reaparecer */
    TRY(fresh(spc));
    struct fat32_node node;
    TRY(open_node("/C.BIN", &node));
    CHECK(fat32_truncate(g_vol, &node, 100) == 0);
    CHECK(fat32_truncate(g_vol, &node, 3u * cs) == 0);
    CHECK(fat32_node_sync(g_vol, &node) == 0);      /* crescer so marca sujo */
    model_pattern(6, 0, 100);
    memset(g_model + 100, 0, 3u * cs - 100u);
    TRY(close_vol());
    TRY(check_image(true));
    TRY(open_vol(false));
    TRY(file_equals("/C.BIN", 3u * cs));

    /* truncate para o mesmo tamanho nao faz nada; crescer um arquivo vazio */
    TRY(open_node("/A.BIN", &node));
    CHECK(fat32_truncate(g_vol, &node, 0) == 0);
    CHECK(fat32_truncate(g_vol, &node, cs + 5u) == 0);
    CHECK(fat32_node_sync(g_vol, &node) == 0);
    memset(g_model, 0, cs + 5u);
    TRY(close_vol());
    TRY(check_image(true));
    TRY(open_vol(false));
    TRY(file_equals("/A.BIN", cs + 5u));
    TRY(close_vol());
    return 0;
}

/* Enche o volume: -ENOSPC sem corromper, e truncate devolve tudo. */
static int test_fill_volume(uint32_t spc) {
    TRY(fresh(spc));
    uint32_t free0 = 0;
    CHECK(fat32_free_clusters(g_vol, &free0) == 0);
    struct fat32_node node;
    TRY(open_node("/A.BIN", &node));
    uint32_t written = 0;
    static uint8_t chunk[65536];
    for (;;) {
        for (uint32_t i = 0; i < sizeof(chunk); i++) chunk[i] = pat(2, written + i);
        ssize_t n = fat32_write(g_vol, &node, written, chunk, sizeof(chunk));
        if (n < 0) {
            CHECK(n == -ENOSPC);
            break;
        }
        CHECK(n > 0);
        written += (uint32_t)n;
    }
    CHECK(written == free0 * csize());
    CHECK(node.info.size == written);
    uint32_t left = 1;
    CHECK(fat32_free_clusters(g_vol, &left) == 0 && left == 0);
    CHECK(fat32_write(g_vol, &node, written, chunk, 1) == -ENOSPC);
    CHECK(fat32_node_sync(g_vol, &node) == 0);
    TRY(close_vol());
    TRY(check_image(true));
    TRY(open_vol(false));
    TRY(open_node("/A.BIN", &node));
    CHECK(node.info.size == written);
    for (uint32_t off = 0; off < written; off += sizeof(chunk)) {   /* le tudo */
        uint32_t n = written - off < sizeof(chunk) ? written - off : sizeof(chunk);
        CHECK(fat32_read(g_vol, &node, off, g_rbuf, n) == (ssize_t)n);
        for (uint32_t i = 0; i < n; i++) CHECK(g_rbuf[i] == pat(2, off + i));
    }
    CHECK(fat32_truncate(g_vol, &node, 0) == 0);
    CHECK(fat32_free_clusters(g_vol, &left) == 0 && left == free0);
    TRY(close_vol());
    TRY(check_image(true));
    CHECK(free_in_image() == free0);
    return 0;
}

/* Volume somente leitura, atributo RO, limite de 4 GiB, O_CREAT. */
static int test_denied(uint32_t spc) {
    format(spc, 0);
    populate();
    TRY(open_vol(true));
    CHECK(fat32_readonly(g_vol));
    struct fat32_node node;
    TRY(open_node("/B.BIN", &node));
    CHECK(fat32_write(g_vol, &node, 0, "x", 1) == -EROFS);
    CHECK(fat32_truncate(g_vol, &node, 0) == -EROFS);
    int err = 0;
    CHECK(fat32_vnode_open(g_vol, "/B.BIN", O_WRONLY, &err) == NULL && err == -EROFS);
    CHECK(fat32_vnode_open(g_vol, "/NOVO.TXT", O_WRONLY | O_CREAT, &err) == NULL &&
          err == -EROFS);
    uint32_t free_clusters = 0;
    CHECK(fat32_free_clusters(g_vol, &free_clusters) == 0 &&
          free_clusters == free_in_image());            /* vem do FSInfo */
    TRY(close_vol());

    TRY(open_vol(false));
    TRY(open_node("/RO.TXT", &node));
    CHECK(fat32_write(g_vol, &node, 0, "x", 1) == -EACCES);
    CHECK(fat32_truncate(g_vol, &node, 0) == -EACCES);
    CHECK(fat32_vnode_open(g_vol, "/RO.TXT", O_RDWR, &err) == NULL && err == -EACCES);
    struct vnode* ro = fat32_vnode_open(g_vol, "/RO.TXT", O_RDONLY, &err);
    CHECK(ro);
    ro->ops->release(ro);
    CHECK(fat32_vnode_open(g_vol, "/", O_WRONLY, &err) == NULL && err == -EISDIR);
    CHECK(fat32_vnode_open(g_vol, "/NOVO.TXT", O_WRONLY | O_CREAT, &err) == NULL &&
          err == -ENOSYS);                          /* create: etapa 3.2 */
    TRY(open_node("/B.BIN", &node));
    CHECK(fat32_write(g_vol, &node, 0xFFFFFFFFu, "x", 1) == -EFBIG);
    CHECK(node.info.size == 1000);
    TRY(close_vol());
    TRY(check_image(true));
    return 0;
}

static void clock_fixed(uint16_t* date, uint16_t* time) {
    *date = 0x5C2Au;
    *time = 0x7000u;
}

/* Camada vnode: truncate no open, vnode compartilhado, flush no release. */
static int test_vnode(uint32_t spc) {
    TRY(fresh(spc));
    fat32_set_clock(clock_fixed);
    int err = 0;
    struct vnode* a = fat32_vnode_open(g_vol, "/B.BIN", O_WRONLY | O_TRUNC, &err);
    CHECK(a && err == 0 && a->size == 0);
    struct vnode* b = fat32_vnode_open(g_vol, "/b.bin", O_RDONLY, &err);
    CHECK(b == a);                                  /* mesmo arquivo, mesmo vnode */
    CHECK(a->ops->write(a, "hello", 5, 0) == 5);
    CHECK(b->size == 5);
    CHECK(a->ops->write(a, "!", 1, 0x100000000ll) == -EFBIG);
    CHECK(a->ops->write(a, "x", 1, -1) == -EINVAL);
    static char got[8];
    CHECK(b->ops->read(b, got, 8, 0) == 5 && memcmp(got, "hello", 5) == 0);
    a->ops->release(a);
    CHECK(fat32_unmount(g_vol) == -EBUSY);          /* ainda aberto por `b` */
    b->ops->release(b);                             /* ultimo: grava a entrada */
    struct fat32_info info;
    CHECK(fat32_lookup(g_vol, "/B.BIN", &info) == 0);
    CHECK(info.size == 5 && info.wdate == 0x5C2Au && info.wtime == 0x7000u &&
          (info.attr & FAT32_ATTR_ARCH));
    TRY(close_vol());
    fat32_set_clock(NULL);
    TRY(check_image(true));
    return 0;
}

/* Sem espelhamento (bit 7 de ext_flags): so a FAT ativa (copia 1) muda. */
static int test_active_fat_only(uint32_t spc) {
    format(spc, 0x81u);
    populate();
    memcpy(g_fat0_snapshot, fat_ptr(0, 0), (size_t)g.fat_sectors * 512u);
    memcpy(fat_ptr(1, 0), fat_ptr(0, 0), (size_t)g.fat_sectors * 512u);
    TRY(open_vol(false));
    struct fat32_node node;
    TRY(open_node("/A.BIN", &node));
    model_pattern(4, 0, 5000);
    CHECK(fat32_write(g_vol, &node, 0, g_model, 5000) == 5000);
    CHECK(fat32_node_sync(g_vol, &node) == 0);
    TRY(close_vol());
    CHECK(memcmp(g_fat0_snapshot, fat_ptr(0, 0), (size_t)g.fat_sectors * 512u) == 0);
    CHECK(fat_get(1, node.info.first_cluster) != 0);
    TRY(open_vol(false));
    TRY(file_equals("/A.BIN", 5000));
    TRY(close_vol());
    return 0;
}

static int run_config(uint32_t spc) {
    TRY(test_write_sizes(spc));
    TRY(test_overwrite_and_hole(spc));
    TRY(test_append_chunks(spc));
    TRY(test_truncate(spc));
    TRY(test_fill_volume(spc));
    TRY(test_denied(spc));
    TRY(test_vnode(spc));
    TRY(test_active_fat_only(spc));
    return 0;
}

int main(void) {
    kheap_init(g_arena, sizeof(g_arena));
    blk_init();
    int r = run_config(1);
    if (!r) r = run_config(2);
    if (r) {
        say("fat32_write_test: falhou na linha ");
        say_number((unsigned)r);
    }
    return r ? 1 : 0;
}
