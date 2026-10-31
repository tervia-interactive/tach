/* Host test: FAT32 somente leitura (src/fs/fat32.c).
 *
 * Modo padrao: monta imagens FAT32 sinteticas em memoria (formatador minimo
 * escrito aqui), com 8.3, nomes longos (LFN, Unicode, par substituto), LFN
 * invalido, entradas apagadas, cadeias fragmentadas/invertidas e FATs
 * corrompidas, e confere BPB, lookup, readdir, leitura e codigos de erro.
 *
 * Modo imagem real: `fat32-test <imagem> <manifesto>` valida uma imagem criada
 * por mkfs.fat + mcopy contra um manifesto (ver scripts/fat32-realimg-test.sh):
 * linhas "F <tamanho> <fnv1a-hex8> <caminho>" e "D <caminho>". */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <drivers/block/blk.h>
#include <fs/fat32.h>

/* So estas quatro chamadas de libc (sem incluir cabecalhos do host, que
 * conflitam com kernel/types.h). */
int open(const char* path, int flags, ...);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int close(int fd);

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

/* ---------------------------------------------------------- saida simples */

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
#define CLUSTERS    65600u          /* >= 65525: continua sendo FAT32 */
#define EOC         0x0FFFFFFFu
#define RESERVED    32u

static uint8_t g_img[IMG_BYTES] __attribute__((aligned(16)));
static struct { uint32_t spc, fat_sectors, total, data_start, next; } g;

static void w16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t* p, uint32_t v) { w16(p, v); w16(p + 2, v >> 16); }

static void fat_set(uint32_t cluster, uint32_t value) {
    for (uint32_t f = 0; f < 2; f++)
        w32(g_img + ((size_t)RESERVED + (size_t)f * g.fat_sectors) * 512u +
            (size_t)cluster * 4u, value);
}

static uint8_t* cluster_ptr(uint32_t cluster) {
    return g_img + ((size_t)g.data_start + (size_t)(cluster - 2u) * g.spc) * 512u;
}

static void format(uint32_t spc) {
    memset(g_img, 0, 8u << 20);
    g.spc = spc;
    g.fat_sectors = (CLUSTERS + 2u + 127u) / 128u;
    g.data_start = RESERVED + 2u * g.fat_sectors;
    g.total = g.data_start + CLUSTERS * spc;
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
    w32(b + 44, 2);
    w16(b + 48, 1);
    w16(b + 50, 6);
    b[66] = 0x29;
    w32(b + 67, 0x12345678u);
    memcpy(b + 71, "TACHVOL    ", 11);
    memcpy(b + 82, "FAT32   ", 8);
    b[510] = 0x55; b[511] = 0xAA;
    fat_set(0, 0x0FFFFFF8u);
    fat_set(1, EOC);
    fat_set(2, EOC);                    /* raiz: um cluster */
    g.next = 3;
}

static uint32_t take_cluster(void) {
    uint32_t cluster = g.next++;
    fat_set(cluster, EOC);
    return cluster;
}

static uint8_t pat(uint32_t seed, uint32_t i) {
    return (uint8_t)(i * 131u + (i >> 8) * 17u + seed * 7u + 3u);
}

/* Cadeia para `size` bytes de padrao. mode 0: contigua; 1: fragmentada (com
 * buracos); 2: ordem de numeros de cluster invertida. 0 se size == 0. */
static uint32_t make_chain(uint32_t size, uint32_t seed, int mode) {
    if (!size) return 0;
    uint32_t csize = g.spc * 512u;
    uint32_t n = (size + csize - 1u) / csize;
    static uint32_t list[1200], order[1200];
    for (uint32_t i = 0; i < n; i++) {
        list[i] = g.next;
        g.next += mode == 1 ? 3u : 1u;
    }
    for (uint32_t i = 0; i < n; i++) order[i] = mode == 2 ? list[n - 1u - i] : list[i];
    for (uint32_t i = 0; i < n; i++) {
        fat_set(order[i], i + 1u < n ? order[i + 1u] : EOC);
        uint32_t bytes = size - i * csize < csize ? size - i * csize : csize;
        for (uint32_t j = 0; j < bytes; j++)
            cluster_ptr(order[i])[j] = pat(seed, i * csize + j);
    }
    return order[0];
}

struct wdir { uint32_t last, used; };

static void put_entry(struct wdir* d, const uint8_t* e) {
    if (d->used == g.spc * 16u) {
        uint32_t next = take_cluster();
        fat_set(d->last, next);
        d->last = next;
        d->used = 0;
    }
    memcpy(cluster_ptr(d->last) + d->used * 32u, e, 32);
    d->used++;
}

static void put_short(struct wdir* d, const char* name11, uint8_t attr,
                      uint8_t nt, uint32_t cluster, uint32_t size) {
    uint8_t e[32];
    memset(e, 0, sizeof(e));
    memcpy(e, name11, 11);
    e[11] = attr;
    e[12] = nt;
    w16(e + 22, 0x6000);
    w16(e + 24, 0x5A21);
    w16(e + 20, cluster >> 16);
    w16(e + 26, cluster & 0xFFFFu);
    w32(e + 28, size);
    put_entry(d, e);
}

static uint8_t csum11(const char* name11) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (uint8_t)(((sum & 1u) << 7) + (sum >> 1) + (uint8_t)name11[i]);
    return sum;
}

static void put_lfn(struct wdir* d, const uint16_t* units, uint32_t n,
                    uint8_t checksum) {
    static const uint8_t offs[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    uint32_t slots = (n + 12u) / 13u;
    uint16_t padded[13u * 20u];
    for (uint32_t i = 0; i < slots * 13u; i++)
        padded[i] = i < n ? units[i] : i == n ? 0 : 0xFFFFu;
    for (uint32_t k = slots; k >= 1u; k--) {
        uint8_t e[32];
        memset(e, 0, sizeof(e));
        e[0] = (uint8_t)(k | (k == slots ? 0x40u : 0u));
        e[11] = 0x0F;
        e[13] = checksum;
        for (uint32_t i = 0; i < 13u; i++) w16(e + offs[i], padded[(k - 1u) * 13u + i]);
        put_entry(d, e);
    }
}

static void put_named(struct wdir* d, const char* longname, const char* name11,
                      uint8_t attr, uint32_t cluster, uint32_t size) {
    uint16_t units[64];
    uint32_t n = (uint32_t)strlen(longname);
    for (uint32_t i = 0; i < n; i++) units[i] = (uint8_t)longname[i];
    put_lfn(d, units, n, csum11(name11));
    put_short(d, name11, attr, 0, cluster, size);
}

static uint32_t make_dir(uint32_t parent, struct wdir* d) {
    uint32_t cluster = take_cluster();
    d->last = cluster;
    d->used = 0;
    put_short(d, ".          ", 0x10, 0, cluster, 0);
    put_short(d, "..         ", 0x10, 0, parent, 0);
    return cluster;
}

static void kill_last(struct wdir* d) {
    cluster_ptr(d->last)[(d->used - 1u) * 32u] = 0xE5;
}

/* Arquivos de conteudo conhecido: caminho, tamanho, semente do padrao. */
struct file_spec { const char* path; uint32_t size; uint32_t seed; };
static const struct file_spec g_files[] = {
    {"/HELLO.TXT", 13, 1},
    {"/readme.md", 700, 2},
    {"/Long File Name With Spaces.txt", 1500, 3},
    {"/empty", 0, 0},
    {"/docs/a.txt", 5000, 4},
    {"/docs/sub/deep.bin", 70000, 5},
    {"/big.bin", 300001, 6},
    {"/rev.bin", 9000, 7},
    {"/frag.bin", 9001, 8},
    {"/\xC3\xBCn\xC3\xAF.txt", 100, 9},
    {"/\xF0\x9F\x98\x80.txt", 600, 10},
    {"/BADCSUM.TXT", 50, 11},
};
#define NFILES (sizeof(g_files) / sizeof(g_files[0]))

static void populate(void) {
    struct wdir root = {2, 0};
    put_short(&root, "TACHVOL    ", 0x08, 0, 0, 0);                  /* rotulo */
    put_short(&root, "HELLO   TXT", 0x20, 0, make_chain(13, 1, 0), 13);
    put_short(&root, "README  MD ", 0x20, 0x18, make_chain(700, 2, 0), 700);
    put_named(&root, "Long File Name With Spaces.txt", "LONGFI~1TXT", 0x20,
              make_chain(1500, 3, 0), 1500);
    put_short(&root, "EMPTY      ", 0x20, 0x08, 0, 0);
    put_short(&root, "GONE    TXT", 0x20, 0, make_chain(10, 99, 0), 10);
    kill_last(&root);                                                /* apagado */

    struct wdir docs, sub;
    uint32_t docs_cluster = make_dir(0, &docs);                      /* pai = raiz */
    put_short(&root, "DOCS       ", 0x10, 0x08, docs_cluster, 0);
    put_short(&docs, "A       TXT", 0x20, 0x18, make_chain(5000, 4, 0), 5000);
    uint32_t sub_cluster = make_dir(docs_cluster, &sub);
    put_short(&docs, "SUB        ", 0x10, 0x08, sub_cluster, 0);
    put_short(&sub, "DEEP    BIN", 0x20, 0x18, make_chain(70000, 5, 1), 70000);

    put_short(&root, "BIG     BIN", 0x20, 0x18, make_chain(300001, 6, 0), 300001);
    put_short(&root, "REV     BIN", 0x20, 0x18, make_chain(9000, 7, 2), 9000);
    put_short(&root, "FRAG    BIN", 0x20, 0x18, make_chain(9001, 8, 1), 9001);

    static const uint16_t uni[] = {0x00FC, 'n', 0x00EF, '.', 't', 'x', 't'};
    put_lfn(&root, uni, 7, csum11("UNI~1   TXT"));
    put_short(&root, "UNI~1   TXT", 0x20, 0, make_chain(100, 9, 0), 100);
    static const uint16_t smile[] = {0xD83D, 0xDE00, '.', 't', 'x', 't'};
    put_lfn(&root, smile, 6, csum11("SMILE~1 TXT"));
    put_short(&root, "SMILE~1 TXT", 0x20, 0, make_chain(600, 10, 0), 600);
    static const uint16_t bad[] = {'b', 'a', 'd', ' ', 'c', 's', 'u', 'm', '.',
                                   't', 'x', 't'};
    put_lfn(&root, bad, 12, (uint8_t)(csum11("BADCSUM TXT") + 1u));  /* errado */
    put_short(&root, "BADCSUM TXT", 0x20, 0, make_chain(50, 11, 0), 50);

    for (uint32_t i = 0; i < 100; i++) {          /* a raiz passa de um cluster */
        char name[12] = "F000    DAT";
        name[1] = (char)('0' + i / 100u);
        name[2] = (char)('0' + (i / 10u) % 10u);
        name[3] = (char)('0' + i % 10u);
        put_short(&root, name, 0x20, 0, make_chain(i, 100u + i, 0), i);
    }

    /* Estruturas corrompidas, usadas por test_corrupt. */
    uint32_t csize = g.spc * 512u;
    put_short(&root, "SHORT   BIN", 0x20, 0, make_chain(csize, 12, 0), csize + 700u);
    static const struct { const char* name; uint32_t value; } broken[] = {
        {"BADLINK0BIN", 0}, {"BADLINK1BIN", 1}, {"BADLINKBBIN", 0x0FFFFFF7u},
        {"BADLINKOBIN", CLUSTERS + 2u},
    };
    for (unsigned i = 0; i < 4; i++) {
        uint32_t first = make_chain(csize, 13, 0);
        fat_set(first, broken[i].value);
        put_short(&root, broken[i].name, 0x20, 0, first, 2u * csize);
    }
    put_short(&root, "NOCHAIN BIN", 0x20, 0, 1, 100);
    uint32_t la = take_cluster(), lb = take_cluster();
    fat_set(la, lb);
    fat_set(lb, la);                              /* diretorio em loop */
    for (uint32_t k = 0; k < g.spc * 16u; k++) {
        cluster_ptr(la)[k * 32u] = 0xE5;
        cluster_ptr(lb)[k * 32u] = 0xE5;
    }
    put_short(&root, "LOOPDIR    ", 0x10, 0x08, la, 0);
}

/* =============================================================== testes */

static struct blkdev* attach(uint64_t sectors) {
    return ramdisk_create("ram0", g_img, (size_t)(sectors * 512u), false);
}

static int mount_expect(uint64_t dev_sectors, int expected) {
    struct blkdev* dev = attach(dev_sectors);
    CHECK(dev);
    struct fat32_vol* vol = NULL;
    int result = fat32_mount(dev, &vol);
    CHECK(result == expected);
    if (result == 0) {
        CHECK(vol && fat32_unmount(vol) == 0);
    } else {
        CHECK(vol == NULL);
    }
    ramdisk_destroy(dev);
    return 0;
}

static int reject_case(int which) {
    format(1);
    switch (which) {
        case 0:  g_img[510] = 0; break;                         /* assinatura */
        case 1:  w16(g_img + 11, 1024); break;                  /* setor 1 KiB */
        case 2:  g_img[13] = 3; break;                          /* spc nao pot. 2 */
        case 3:  g_img[13] = 0; break;
        case 4:  w16(g_img + 14, 0); break;                     /* sem reservados */
        case 5:  g_img[16] = 0; break;                          /* 0 FATs */
        case 6:  g_img[16] = 5; break;
        case 7:  w16(g_img + 17, 512); break;                   /* raiz FAT16 */
        case 8:  w16(g_img + 22, 5); break;                     /* fat_size_16 */
        case 9:  w32(g_img + 32, 32u + 2u * g.fat_sectors + 1000u); break; /* poucos clusters */
        case 10: w32(g_img + 36, 10); break;                    /* FAT pequena demais */
        case 11: w32(g_img + 44, 0); break;                     /* raiz invalida */
        case 12: w32(g_img + 44, 1); break;
        case 13: w32(g_img + 44, CLUSTERS + 2u); break;
        case 14: w16(g_img + 40, 0x80u | 3u); break;            /* FAT ativa inexistente */
        case 15: w32(g_img + 36, 0); break;
        default: break;
    }
    return mount_expect(g.total, -EINVAL);
}

static int test_mount_rejects(void) {
    for (int i = 0; i <= 15; i++) {
        int line = reject_case(i);
        if (line) { say("reject_case "); say_number((unsigned)i); return line; }
    }
    format(1);
    TRY(mount_expect(g.total - 1u, -EINVAL));       /* particao menor que o BPB */
    TRY(mount_expect(g.total, 0));
    return 0;
}

/* Com a FAT 1 marcada como unica ativa, a FAT 0 pode estar destruida. */
static int test_active_fat(void) {
    format(1);
    populate();
    w16(g_img + 40, 0x80u | 1u);
    memset(g_img + (size_t)RESERVED * 512u, 0, (size_t)g.fat_sectors * 512u);
    struct blkdev* dev = attach(g.total);
    CHECK(dev);
    struct fat32_vol* vol = NULL;
    CHECK(fat32_mount(dev, &vol) == 0);
    struct fat32_info info;
    CHECK(fat32_lookup(vol, "/docs/sub/deep.bin", &info) == 0 && info.size == 70000);
    struct fat32_node node;
    fat32_node_init(&node, &info);
    static uint8_t buf[70000];
    CHECK(fat32_read(vol, &node, 0, buf, sizeof(buf)) == 70000);
    for (uint32_t i = 0; i < 70000; i++) CHECK(buf[i] == pat(5, i));
    CHECK(fat32_unmount(vol) == 0);
    ramdisk_destroy(dev);
    return 0;
}

static int test_lookup(struct fat32_vol* vol) {
    struct fat32_info in;
    CHECK(fat32_lookup(vol, "/", &in) == 0 && (in.attr & FAT32_ATTR_DIR));
    CHECK(fat32_lookup(vol, "", &in) == 0 && (in.attr & FAT32_ATTR_DIR));
    CHECK(fat32_lookup(vol, "/HELLO.TXT", &in) == 0 && in.size == 13 &&
          strcmp(in.name, "HELLO.TXT") == 0 && strcmp(in.short_name, "HELLO.TXT") == 0);
    CHECK(fat32_lookup(vol, "/hello.txt", &in) == 0 && in.size == 13);
    CHECK(fat32_lookup(vol, "/Hello.Txt", &in) == 0);
    CHECK(fat32_lookup(vol, "//docs///a.txt", &in) == 0 && in.size == 5000 &&
          strcmp(in.name, "a.txt") == 0 && strcmp(in.short_name, "A.TXT") == 0);
    CHECK(fat32_lookup(vol, "docs/sub/deep.bin", &in) == 0 && in.size == 70000);
    CHECK(fat32_lookup(vol, "./docs/./a.txt", &in) == 0);
    CHECK(fat32_lookup(vol, "/DOCS/SUB/DEEP.BIN", &in) == 0);
    CHECK(fat32_lookup(vol, "/readme.md", &in) == 0 &&
          strcmp(in.name, "readme.md") == 0 && strcmp(in.short_name, "README.MD") == 0);
    CHECK(fat32_lookup(vol, "/long file name with spaces.TXT", &in) == 0 &&
          strcmp(in.name, "Long File Name With Spaces.txt") == 0 && in.size == 1500);
    CHECK(fat32_lookup(vol, "/LONGFI~1.TXT", &in) == 0 && in.size == 1500);
    CHECK(fat32_lookup(vol, "/empty", &in) == 0 && in.size == 0 && in.first_cluster == 0);
    CHECK(fat32_lookup(vol, "/\xC3\xBCn\xC3\xAF.txt", &in) == 0 && in.size == 100);
    CHECK(fat32_lookup(vol, "/\xF0\x9F\x98\x80.txt", &in) == 0 && in.size == 600);
    CHECK(fat32_lookup(vol, "/BADCSUM.TXT", &in) == 0 &&
          strcmp(in.name, "BADCSUM.TXT") == 0);           /* LFN com checksum errado */
    CHECK(fat32_lookup(vol, "/bad csum.txt", &in) == -ENOENT);
    CHECK(fat32_lookup(vol, "/docs", &in) == 0 && (in.attr & FAT32_ATTR_DIR) && in.size == 0);
    CHECK(fat32_lookup(vol, "/GONE.TXT", &in) == -ENOENT);
    CHECK(fat32_lookup(vol, "/TACHVOL", &in) == -ENOENT);
    CHECK(fat32_lookup(vol, "/docs/missing", &in) == -ENOENT);
    CHECK(fat32_lookup(vol, "/HELLO.TXT/x", &in) == -ENOTDIR);
    CHECK(fat32_lookup(vol, "/docs/../HELLO.TXT", &in) == -ENOENT);
    CHECK(fat32_lookup(vol, "/..", &in) == -ENOENT);
    static char toolong[300];
    toolong[0] = '/';
    memset(toolong + 1, 'a', 256);
    toolong[257] = '\0';
    CHECK(fat32_lookup(vol, toolong, &in) == -ENAMETOOLONG);
    CHECK(fat32_lookup(NULL, "/", &in) == -EINVAL && fat32_lookup(vol, NULL, &in) == -EINVAL);
    return 0;
}

static int verify_range(struct fat32_vol* vol, struct fat32_node* node,
                        uint32_t seed, uint32_t size, uint32_t off, uint32_t len) {
    static uint8_t buf[310000];
    CHECK(len <= sizeof(buf));
    ssize_t got = fat32_read(vol, node, off, buf, len);
    uint32_t expect = off >= size ? 0 : (len > size - off ? size - off : len);
    CHECK(got >= 0 && (uint32_t)got == expect);
    for (uint32_t i = 0; i < expect; i++) CHECK(buf[i] == pat(seed, off + i));
    return 0;
}

static int test_files(struct fat32_vol* vol) {
    static const uint32_t chunks[] = {1000, 512, 4097, 777, 65000, 1, 513};
    uint32_t rng = 12345u;
    for (size_t f = 0; f < NFILES; f++) {
        const struct file_spec* spec = &g_files[f];
        struct fat32_info info;
        CHECK(fat32_lookup(vol, spec->path, &info) == 0);
        CHECK(info.size == spec->size && !(info.attr & FAT32_ATTR_DIR));
        struct fat32_node node;

        fat32_node_init(&node, &info);                 /* tudo de uma vez */
        TRY(verify_range(vol, &node, spec->seed, spec->size, 0, spec->size));

        fat32_node_init(&node, &info);                 /* sequencial em pedacos */
        uint32_t off = 0;
        for (unsigned k = 0; off < spec->size; k++) {
            uint32_t len = chunks[k % 7u];
            if (len == 1 && off > 2000) len = 999;     /* 1 byte so no comeco */
            TRY(verify_range(vol, &node, spec->seed, spec->size, off, len));
            off += len < spec->size - off ? len : spec->size - off;
        }

        TRY(verify_range(vol, &node, spec->seed, spec->size, spec->size, 10));
        TRY(verify_range(vol, &node, spec->seed, spec->size, spec->size + 5000u, 10));
        if (spec->size) {
            TRY(verify_range(vol, &node, spec->seed, spec->size, spec->size - 1u, 100));
            TRY(verify_range(vol, &node, spec->seed, spec->size, 0, 1));
        }
        for (int k = 0; k < 60; k++) {                 /* saltos (invalida o cache) */
            rng = rng * 1103515245u + 12345u;
            uint32_t at = (rng >> 8) % (spec->size + 1u);
            rng = rng * 1103515245u + 12345u;
            uint32_t len = (rng >> 8) % 6000u;
            TRY(verify_range(vol, &node, spec->seed, spec->size, at, len));
        }
    }
    struct fat32_info dir;
    struct fat32_node node;
    CHECK(fat32_lookup(vol, "/docs", &dir) == 0);
    fat32_node_init(&node, &dir);
    char c;
    CHECK(fat32_read(vol, &node, 0, &c, 1) == -EISDIR);
    return 0;
}

#define MAX_NAMES 160
static char g_names[MAX_NAMES][80];

static int list_dir(struct fat32_vol* vol, const char* path, size_t* count) {
    struct fat32_info info;
    CHECK(fat32_lookup(vol, path, &info) == 0);
    struct fat32_node node;
    fat32_node_init(&node, &info);
    size_t n = 0;
    for (;;) {
        struct fat32_info entry;
        int result = fat32_readdir(vol, &node, n, &entry);
        CHECK(result >= 0);
        if (result == 0) break;
        CHECK(n < MAX_NAMES && strlen(entry.name) < sizeof(g_names[0]));
        strcpy(g_names[n++], entry.name);
    }
    *count = n;
    return 0;
}

static bool has_name(size_t count, const char* name) {
    for (size_t i = 0; i < count; i++)
        if (strcmp(g_names[i], name) == 0) return true;
    return false;
}

static int test_dirs(struct fat32_vol* vol) {
    size_t count = 0;
    TRY(list_dir(vol, "/docs", &count));
    CHECK(count == 2 && has_name(count, "a.txt") && has_name(count, "sub"));
    TRY(list_dir(vol, "/docs/sub", &count));
    CHECK(count == 1 && has_name(count, "deep.bin"));

    TRY(list_dir(vol, "/", &count));
    CHECK(count > 110);
    CHECK(has_name(count, "HELLO.TXT") && has_name(count, "readme.md") &&
          has_name(count, "Long File Name With Spaces.txt") &&
          has_name(count, "empty") && has_name(count, "docs") &&
          has_name(count, "big.bin") && has_name(count, "\xC3\xBCn\xC3\xAF.txt") &&
          has_name(count, "\xF0\x9F\x98\x80.txt") && has_name(count, "BADCSUM.TXT") &&
          has_name(count, "F000.DAT") && has_name(count, "F099.DAT"));
    CHECK(!has_name(count, "GONE.TXT") && !has_name(count, "TACHVOL") &&
          !has_name(count, ".") && !has_name(count, ".."));
    for (size_t i = 0; i < count; i++)             /* sem duplicatas */
        for (size_t j = i + 1; j < count; j++) CHECK(strcmp(g_names[i], g_names[j]) != 0);

    /* Acesso fora de ordem: o cursor em cache tem que reiniciar sozinho. */
    struct fat32_info info, entry;
    struct fat32_node node;
    CHECK(fat32_lookup(vol, "/", &info) == 0);
    fat32_node_init(&node, &info);
    static const size_t order[] = {7, 2, 50, 3, 3, 90, 0, 1};
    for (size_t k = 0; k < sizeof(order) / sizeof(order[0]); k++) {
        CHECK(fat32_readdir(vol, &node, order[k], &entry) == 1);
        CHECK(strcmp(entry.name, g_names[order[k]]) == 0);
    }
    CHECK(fat32_readdir(vol, &node, count, &entry) == 0);
    CHECK(fat32_readdir(vol, &node, count + 5u, &entry) == 0);
    CHECK(fat32_readdir(vol, &node, 1, &entry) == 1 && strcmp(entry.name, g_names[1]) == 0);

    fat32_lookup(vol, "/HELLO.TXT", &info);
    fat32_node_init(&node, &info);
    CHECK(fat32_readdir(vol, &node, 0, &entry) == -ENOTDIR);
    return 0;
}

static int test_corrupt(struct fat32_vol* vol) {
    uint32_t csize = fat32_cluster_size(vol);
    static const char* const paths[] = {"/SHORT.BIN", "/BADLINK0.BIN", "/BADLINK1.BIN",
                                        "/BADLINKB.BIN", "/BADLINKO.BIN"};
    static uint8_t buf[4096 * 4];
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        struct fat32_info info;
        CHECK(fat32_lookup(vol, paths[i], &info) == 0);
        struct fat32_node node;
        fat32_node_init(&node, &info);
        uint32_t ask = csize + (i ? csize : 700u);
        CHECK(ask <= sizeof(buf));
        CHECK(fat32_read(vol, &node, 0, buf, ask) == (ssize_t)csize);   /* parcial */
        CHECK(fat32_read(vol, &node, csize, buf, 10) == -EIO);
        CHECK(fat32_read(vol, &node, 0, buf, 10) == 10);                /* 1o cluster ok */
    }
    struct fat32_info info;
    struct fat32_node node;
    CHECK(fat32_lookup(vol, "/NOCHAIN.BIN", &info) == 0);
    fat32_node_init(&node, &info);
    CHECK(fat32_read(vol, &node, 0, buf, 10) == -EIO);

    CHECK(fat32_lookup(vol, "/loopdir", &info) == 0 && (info.attr & FAT32_ATTR_DIR));
    fat32_node_init(&node, &info);
    struct fat32_info entry;
    CHECK(fat32_readdir(vol, &node, 0, &entry) == -EIO);       /* cadeia em loop */
    CHECK(fat32_lookup(vol, "/loopdir/x", &info) == -EIO);
    return 0;
}

static int test_vnode(struct fat32_vol* vol) {
    int error = 99;
    struct vnode* v = fat32_vnode_open(vol, "/docs/a.txt", O_RDONLY, &error);
    CHECK(v && error == 0 && v->type == VNODE_FILE && v->size == 5000 &&
          strcmp(v->name, "a.txt") == 0 && strcmp(v->path, "/docs/a.txt") == 0);
    uint8_t b[100];
    CHECK(v->ops->read(v, b, 100, 4950) == 50);
    for (uint32_t i = 0; i < 50; i++) CHECK(b[i] == pat(4, 4950u + i));
    CHECK(v->ops->read(v, b, 10, 5000) == 0);
    CHECK(v->ops->read(v, b, 10, -1) == -EINVAL);
    CHECK(v->ops->write(v, b, 1, 0) == -EROFS);
    CHECK(fat32_unmount(vol) == -EBUSY);
    v->ops->dup(v);
    v->ops->release(v);
    CHECK(fat32_unmount(vol) == -EBUSY);
    v->ops->release(v);                                /* liberado aqui */

    CHECK(fat32_vnode_open(vol, "/docs/a.txt", O_WRONLY, &error) == NULL && error == -EROFS);
    CHECK(fat32_vnode_open(vol, "/docs/a.txt", O_RDWR, &error) == NULL && error == -EROFS);
    CHECK(fat32_vnode_open(vol, "/docs/a.txt", O_RDONLY | O_TRUNC, &error) == NULL &&
          error == -EROFS);
    CHECK(fat32_vnode_open(vol, "/new.txt", O_WRONLY | O_CREAT, &error) == NULL &&
          error == -EROFS);
    CHECK(fat32_vnode_open(vol, "/nope", O_RDONLY, &error) == NULL && error == -ENOENT);
    CHECK(fat32_vnode_open(vol, "/docs", O_WRONLY, &error) == NULL && error == -EISDIR);
    static char longpath[140];
    memset(longpath, 'x', sizeof(longpath) - 1u);
    longpath[0] = '/';
    CHECK(fat32_vnode_open(vol, longpath, O_RDONLY, &error) == NULL &&
          error == -ENAMETOOLONG);
    v = fat32_vnode_open(vol, "/HELLO.TXT", O_RDONLY | O_CREAT, &error);
    CHECK(v && error == 0);
    v->ops->release(v);

    struct vnode* dir = fat32_vnode_open(vol, "/docs", O_RDONLY, &error);
    CHECK(dir && dir->type == VNODE_DIR);
    struct dirent de;
    CHECK(fat32_vnode_readdir(dir, 0, &de) == 1 && strcmp(de.d_name, "a.txt") == 0 &&
          de.d_type == VNODE_FILE && de.d_off == 1);
    CHECK(fat32_vnode_readdir(dir, 1, &de) == 1 && strcmp(de.d_name, "sub") == 0 &&
          de.d_type == VNODE_DIR);
    CHECK(fat32_vnode_readdir(dir, 2, &de) == 0);
    CHECK(dir->ops->read(dir, b, 1, 0) == -EISDIR);
    dir->ops->release(dir);
    v = fat32_vnode_open(vol, "/HELLO.TXT", O_RDONLY, &error);
    CHECK(v && fat32_vnode_readdir(v, 0, &de) == -ENOTDIR);
    v->ops->release(v);
    return 0;
}

static int run_config(uint32_t spc) {
    format(spc);
    populate();
    struct blkdev* dev = attach(g.total);
    CHECK(dev);
    struct fat32_vol* vol = NULL;
    CHECK(fat32_mount(dev, &vol) == 0 && vol);
    CHECK(strcmp(fat32_label(vol), "TACHVOL") == 0);
    CHECK(fat32_cluster_count(vol) == CLUSTERS && fat32_cluster_size(vol) == spc * 512u);
    TRY(test_lookup(vol));
    TRY(test_files(vol));
    TRY(test_dirs(vol));
    TRY(test_corrupt(vol));
    TRY(test_vnode(vol));
    CHECK(fat32_unmount(vol) == 0);
    ramdisk_destroy(dev);
    return 0;
}

/* ======================================================== imagem real */

static char g_manifest[1u << 20];
static uint8_t g_chunk[3001];

static uint32_t fnv1a(uint32_t hash, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; i++) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}

static size_t load(const char* path, void* dest, size_t capacity) {
    int fd = open(path, 0);
    if (fd < 0) return 0;
    size_t total = 0;
    for (;;) {
        size_t room = capacity - total;
        if (!room) break;
        ssize_t n = read(fd, (uint8_t*)dest + total, room < (1u << 20) ? room : (1u << 20));
        if (n <= 0) break;
        total += (size_t)n;
    }
    close(fd);
    return total;
}

static unsigned walk(struct fat32_vol* vol, const char* path, int* error) {
    struct fat32_info info;
    if (fat32_lookup(vol, path, &info) < 0) { *error = __LINE__; return 0; }
    struct fat32_node node;
    fat32_node_init(&node, &info);
    unsigned count = 0;
    for (size_t i = 0;; i++) {
        struct fat32_info entry;
        int result = fat32_readdir(vol, &node, i, &entry);
        if (result < 0) { *error = __LINE__; return count; }
        if (result == 0) break;
        count++;
        if (entry.attr & FAT32_ATTR_DIR) {
            char child[400];
            size_t base = strlen(path);
            if (base + strlen(entry.name) + 2 > sizeof(child)) { *error = __LINE__; return count; }
            strcpy(child, path);
            if (base == 0 || child[base - 1] != '/') child[base++] = '/';
            strcpy(child + base, entry.name);
            count += walk(vol, child, error);
        }
    }
    return count;
}

static int run_real(const char* image, const char* manifest) {
    size_t bytes = load(image, g_img, sizeof(g_img));
    CHECK(bytes >= 512);
    size_t mlen = load(manifest, g_manifest, sizeof(g_manifest) - 1u);
    CHECK(mlen > 0);
    g_manifest[mlen] = '\0';
    struct blkdev* dev = ramdisk_create("img0", g_img, bytes & ~(size_t)511, true);
    CHECK(dev);
    struct fat32_vol* vol = NULL;
    CHECK(fat32_mount(dev, &vol) == 0);

    unsigned expected = 0;
    int failed = 0;
    for (char* line = g_manifest; *line;) {
        char* next = line;
        while (*next && *next != '\n') next++;
        if (*next) *next++ = '\0';
        if (line[0] == 'F' || line[0] == 'D') {
            char* p = line + 2;
            uint32_t size = 0, want = 0;
            if (line[0] == 'F') {
                while (*p >= '0' && *p <= '9') size = size * 10u + (uint32_t)(*p++ - '0');
                p++;
                while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))
                    want = want * 16u + (uint32_t)(*p <= '9' ? *p - '0' : *p - 'a' + 10), p++;
                p++;
            }
            expected++;
            struct fat32_info info;
            int result = fat32_lookup(vol, p, &info);
            bool ok = result == 0;
            if (ok && line[0] == 'D') ok = (info.attr & FAT32_ATTR_DIR) != 0;
            if (ok && line[0] == 'F') {
                ok = !(info.attr & FAT32_ATTR_DIR) && info.size == size;
                struct fat32_node node;
                fat32_node_init(&node, &info);
                uint32_t hash = 2166136261u, off = 0;
                while (ok && off < size) {
                    ssize_t got = fat32_read(vol, &node, off, g_chunk, sizeof(g_chunk));
                    if (got <= 0) { ok = false; break; }
                    hash = fnv1a(hash, g_chunk, (size_t)got);
                    off += (uint32_t)got;
                }
                if (ok) ok = hash == want;
            }
            if (!ok) { say("FALHA: "); say(p); say("\n"); failed++; }
        }
        line = next;
    }
    int error = 0;
    unsigned seen = walk(vol, "/", &error);
    CHECK(error == 0);
    if (seen != expected) {
        say("entradas na imagem: "); say_number(seen);
        say("entradas no manifesto: "); say_number(expected);
        failed++;
    }
    CHECK(failed == 0);
    CHECK(fat32_unmount(vol) == 0);
    ramdisk_destroy(dev);
    say("imagem real ok, entradas: ");
    say_number(expected);
    return 0;
}

int main(int argc, char** argv) {
    kheap_init(g_arena, sizeof(g_arena));
    blk_init();
    int r = 0;
    if (argc >= 3) {
        r = run_real(argv[1], argv[2]);
    } else {
        r = test_mount_rejects();
        if (!r) r = test_active_fat();
        if (!r) r = run_config(1);
        if (!r) r = run_config(2);
    }
    if (r) {
        say("fat32_test: falhou na linha ");
        say_number((unsigned)r);
    }
    return r ? 1 : 0;
}
