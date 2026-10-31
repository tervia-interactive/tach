/* Host test: block layer (cache, partitions, ramdisk) + ATA PIO driver
 * against a simulated legacy IDE controller. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <hal/io.h>
#include <drivers/block/blk.h>
#include <drivers/block/ata.h>

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

/* ===================================================== IDE controller sim */

#define SIM_SECTORS 4096u                 /* armazenamento real: 2 MiB */
static uint8_t g_sim_disk[SIM_SECTORS * 512];
static uint64_t g_sim_reported;           /* tamanho anunciado no IDENTIFY */
static bool g_sim_lba48;
static bool g_sim_present;
static uint64_t g_sim_last_lba;
static unsigned g_sim_flushes, g_sim_cmds;

static struct {
    uint8_t drive;
    uint8_t count[2], lba[3][2];          /* [0]=atual, [1]=HOB */
    int mode;                             /* 0 idle, 1 read, 2 write, 3 id */
    uint64_t lba_cur;
    uint32_t sectors_left;
    uint16_t buf[256];
    uint32_t word;
} g_ide;

static uint64_t sim_index(uint64_t lba) { return lba % SIM_SECTORS; }

static void sim_fill(void) {
    memcpy(g_ide.buf, g_sim_disk + sim_index(g_ide.lba_cur) * 512, 512);
    g_ide.word = 0;
}

static void sim_command(uint8_t cmd) {
    g_sim_cmds++;
    bool slave = (g_ide.drive & 0x10) != 0;
    if (slave || !g_sim_present) { g_ide.mode = 0; return; }
    uint32_t count = g_ide.count[0];
    uint64_t lba28 = (uint64_t)g_ide.lba[0][0] | (uint64_t)g_ide.lba[1][0] << 8 |
                     (uint64_t)g_ide.lba[2][0] << 16 |
                     (uint64_t)(g_ide.drive & 0x0F) << 24;
    uint64_t lba48 = (uint64_t)g_ide.lba[0][0] | (uint64_t)g_ide.lba[1][0] << 8 |
                     (uint64_t)g_ide.lba[2][0] << 16 |
                     (uint64_t)g_ide.lba[0][1] << 24 |
                     (uint64_t)g_ide.lba[1][1] << 32 |
                     (uint64_t)g_ide.lba[2][1] << 40;
    switch (cmd) {
    case 0xEC:
        memset(g_ide.buf, 0, sizeof(g_ide.buf));
        g_ide.buf[60] = (uint16_t)(g_sim_reported & 0xFFFF);
        g_ide.buf[61] = (uint16_t)((g_sim_reported >> 16) & 0xFFFF);
        if (g_sim_lba48) {
            g_ide.buf[83] = 1u << 10;
            g_ide.buf[100] = (uint16_t)g_sim_reported;
            g_ide.buf[101] = (uint16_t)(g_sim_reported >> 16);
            g_ide.buf[102] = (uint16_t)(g_sim_reported >> 32);
            g_ide.buf[103] = (uint16_t)(g_sim_reported >> 48);
        }
        g_ide.mode = 3; g_ide.word = 0; g_ide.sectors_left = 1;
        break;
    case 0x20: case 0x24:
        g_ide.lba_cur = cmd == 0x20 ? lba28 : lba48;
        g_sim_last_lba = g_ide.lba_cur;
        if (cmd == 0x20) g_ide.sectors_left = count ? count : 256;
        else g_ide.sectors_left = (uint32_t)g_ide.count[1] << 8 | count;
        g_ide.mode = 1;
        sim_fill();
        break;
    case 0x30: case 0x34:
        g_ide.lba_cur = cmd == 0x30 ? lba28 : lba48;
        g_sim_last_lba = g_ide.lba_cur;
        if (cmd == 0x30) g_ide.sectors_left = count ? count : 256;
        else g_ide.sectors_left = (uint32_t)g_ide.count[1] << 8 | count;
        g_ide.mode = 2; g_ide.word = 0;
        break;
    case 0xE7: case 0xEA:
        g_sim_flushes++;
        g_ide.mode = 0;
        break;
    default: g_ide.mode = 0; break;
    }
}

static bool is_primary(uint16_t port) {
    return (port >= 0x1F0 && port <= 0x1F7) || port == 0x3F6;
}
static bool is_secondary(uint16_t port) {
    return (port >= 0x170 && port <= 0x177) || port == 0x376;
}

uint8_t hal_port_in(uint16_t port) {
    if (is_secondary(port)) return 0xFF;               /* barramento flutuando */
    if (!is_primary(port)) return 0;
    uint16_t reg = port == 0x3F6 ? 7 : (uint16_t)(port - 0x1F0);
    bool slave = (g_ide.drive & 0x10) != 0;
    if (reg == 7) {
        if (slave || !g_sim_present) return 0x00;
        uint8_t st = 0x40;
        if (g_ide.mode != 0 && g_ide.sectors_left) st |= 0x08;
        return st;
    }
    return 0;                                          /* LBA1/LBA2 = ATA */
}

void hal_port_out(uint16_t port, uint8_t val) {
    if (!is_primary(port)) return;
    uint16_t reg = port == 0x3F6 ? 99 : (uint16_t)(port - 0x1F0);
    switch (reg) {
    case 2: g_ide.count[1] = g_ide.count[0]; g_ide.count[0] = val; break;
    case 3: case 4: case 5: {
        int i = reg - 3;
        g_ide.lba[i][1] = g_ide.lba[i][0];
        g_ide.lba[i][0] = val;
        break;
    }
    case 6: g_ide.drive = val; break;
    case 7: sim_command(val); break;
    default: break;
    }
}

uint16_t hal_port_in16(uint16_t port) {
    if (port != 0x1F0 || g_ide.mode == 0 || g_ide.mode == 2) return 0;
    uint16_t value = g_ide.buf[g_ide.word++];
    if (g_ide.word == 256) {
        g_ide.sectors_left--;
        if (g_ide.mode == 1 && g_ide.sectors_left) {
            g_ide.lba_cur++;
            sim_fill();
        } else if (!g_ide.sectors_left) {
            g_ide.mode = 0;
        } else {
            g_ide.word = 0;
        }
    }
    return value;
}

void hal_port_out16(uint16_t port, uint16_t val) {
    if (port != 0x1F0 || g_ide.mode != 2) return;
    g_ide.buf[g_ide.word++] = val;
    if (g_ide.word == 256) {
        memcpy(g_sim_disk + sim_index(g_ide.lba_cur) * 512, g_ide.buf, 512);
        g_ide.lba_cur++;
        g_ide.word = 0;
        if (--g_ide.sectors_left == 0) g_ide.mode = 0;
    }
}

uint32_t hal_mmio_read(volatile void* addr) { return *(volatile uint32_t*)addr; }
void hal_mmio_write(volatile void* addr, uint32_t val) {
    *(volatile uint32_t*)addr = val;
}

/* ============================================================== helpers */

static void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static void put64(uint8_t* p, uint64_t v) { put32(p, (uint32_t)v); put32(p + 4, (uint32_t)(v >> 32)); }

static void mbr_entry(uint8_t* sector, int slot, uint8_t boot, uint8_t type,
                      uint32_t start, uint32_t count) {
    uint8_t* e = sector + 446 + slot * 16;
    e[0] = boot; e[4] = type;
    put32(e + 8, start); put32(e + 12, count);
}

static int test_basics(void) {
    CHECK(blk_crc32(0, "123456789", 9) == 0xCBF43926u);
    CHECK(blk_crc32(blk_crc32(0, "1234", 4), "56789", 5) == 0xCBF43926u);

    static uint8_t backing[256 * 512];
    struct blkdev* ram = ramdisk_create("ram0", backing, sizeof(backing), false);
    CHECK(ram && ram->sectors == 256);
    CHECK(blk_find("ram0") == ram && blk_count() == 1 && blk_get(0) == ram);
    CHECK(ramdisk_create("ram0", backing, sizeof(backing), false) == NULL);

    uint8_t a[1024], b[1024];
    for (int i = 0; i < 1024; i++) a[i] = (uint8_t)(i * 7 + 3);
    CHECK(blk_write(ram, 10, 2, a) == 0);
    /* write-back: o armazenamento so recebe os dados apos o flush */
    CHECK(backing[10 * 512] == 0);
    CHECK(blk_read(ram, 10, 2, b) == 0 && memcmp(a, b, 1024) == 0);
    CHECK(blk_flush(ram) == 0);
    CHECK(memcmp(backing + 10 * 512, a, 1024) == 0);

    /* leitura direta de area nao cacheada + setor cacheado no meio */
    for (int i = 0; i < 512; i++) backing[20 * 512 + i] = 0xAA;
    for (int i = 0; i < 512; i++) backing[22 * 512 + i] = 0xCC;
    uint8_t mid[512];
    memset(mid, 0xBB, sizeof(mid));
    CHECK(blk_write(ram, 21, 1, mid) == 0);          /* sujo, so no cache */
    uint8_t run[3 * 512];
    CHECK(blk_read(ram, 20, 3, run) == 0);
    CHECK(run[0] == 0xAA && run[512] == 0xBB && run[1024] == 0xCC);

    /* limites */
    CHECK(blk_read(ram, 256, 1, b) == -EINVAL);
    CHECK(blk_read(ram, 255, 2, b) == -EINVAL);
    CHECK(blk_read(ram, 0, 0, b) == -EINVAL);
    CHECK(blk_write(ram, 1ull << 63, 1, a) == -EINVAL);

    /* bytes nao alinhados atravessando setores */
    uint8_t pattern[700];
    for (int i = 0; i < 700; i++) pattern[i] = (uint8_t)(i ^ 0x5A);
    CHECK(blk_write_bytes(ram, 100 * 512 + 300, pattern, 700) == 0);
    uint8_t back[700];
    CHECK(blk_read_bytes(ram, 100 * 512 + 300, back, 700) == 0);
    CHECK(memcmp(pattern, back, 700) == 0);
    CHECK(blk_read_bytes(ram, 256 * 512 - 4, back, 8) == -EINVAL);

    /* bread / bnew / bdirty */
    struct blkbuf* buf;
    CHECK(bread(ram, 100, &buf) == 0);
    CHECK(buf->data[300] == pattern[0]);
    brelse(buf);
    CHECK(bnew(ram, 100, &buf) == 0);
    for (int i = 0; i < 512; i++) CHECK(buf->data[i] == 0);   /* zerado */
    buf->data[0] = 0x77;
    brelse(buf);
    CHECK(blk_flush(ram) == 0);
    CHECK(backing[100 * 512] == 0x77 && backing[100 * 512 + 300] == 0);
    CHECK(bread(ram, 256, &buf) == -EINVAL);

    CHECK(blk_unregister(ram) == 0 && blk_find("ram0") == NULL);
    return 0;
}

static int test_cache_pressure(void) {
    static uint8_t backing[1024 * 512];
    struct blkdev* ram = ramdisk_create("ram1", backing, sizeof(backing), false);
    CHECK(ram);
    uint8_t sec[512];
    for (uint32_t i = 0; i < 600; i++) {            /* >> 128 buffers */
        memset(sec, (int)(i & 0xFF), sizeof(sec));
        sec[1] = (uint8_t)(i >> 8);
        CHECK(blk_write(ram, i, 1, sec) == 0);
    }
    for (uint32_t i = 0; i < 600; i++) {
        CHECK(blk_read(ram, i, 1, sec) == 0);
        CHECK(sec[0] == (uint8_t)(i & 0xFF) && sec[1] == (uint8_t)(i >> 8) &&
              sec[2] == (uint8_t)(i & 0xFF));
    }
    CHECK(blk_flush(ram) == 0);
    for (uint32_t i = 0; i < 600; i++)
        CHECK(backing[i * 512] == (uint8_t)(i & 0xFF));

    /* todos os buffers presos -> -EBUSY, e libera depois */
    struct blkbuf* pinned[128];
    int n = 0;
    for (; n < 128; n++) CHECK(bread(ram, 700 + (uint32_t)n, &pinned[n]) == 0);
    struct blkbuf* extra;
    CHECK(bread(ram, 900, &extra) == -EBUSY);
    CHECK(blk_unregister(ram) == -EBUSY);
    for (int i = 0; i < n; i++) brelse(pinned[i]);
    CHECK(bread(ram, 900, &extra) == 0);
    brelse(extra);

    /* somente leitura */
    struct blkdev* ro = ramdisk_create("rom0", backing, 512 * 8, true);
    CHECK(ro);
    CHECK(blk_write(ro, 0, 1, sec) == -EROFS);
    CHECK(bnew(ro, 0, &extra) == -EROFS);
    CHECK(blk_read(ro, 0, 1, sec) == 0);
    CHECK(blk_unregister(ro) == 0);
    CHECK(blk_unregister(ram) == 0);
    return 0;
}

static int test_mbr(void) {
    static uint8_t backing[2048 * 512];
    memset(backing, 0, sizeof(backing));
    uint8_t* mbr = backing;
    mbr_entry(mbr, 0, 0x80, 0x0C, 64, 512);          /* primaria FAT32 */
    mbr_entry(mbr, 1, 0x00, 0x83, 1024, 256);        /* primaria Linux */
    mbr_entry(mbr, 2, 0x00, 0x05, 1280, 640);        /* extendida */
    mbr_entry(mbr, 3, 0x01, 0x83, 1, 1);             /* boot flag invalido: ignorada */
    put16(mbr + 510, 0xAA55);
    uint8_t* ebr1 = backing + 1280 * 512;
    mbr_entry(ebr1, 0, 0, 0x0B, 32, 100);            /* logica 1 @1312 */
    mbr_entry(ebr1, 1, 0, 0x05, 200, 400);           /* proximo EBR @1480 */
    put16(ebr1 + 510, 0xAA55);
    uint8_t* ebr2 = backing + (1280 + 200) * 512;
    mbr_entry(ebr2, 0, 0, 0x83, 16, 50);             /* logica 2 @1496 */
    put16(ebr2 + 510, 0xAA55);

    struct blkdev* disk = ramdisk_create("ram2", backing, sizeof(backing), false);
    CHECK(disk);
    CHECK(blk_scan_partitions(disk) == 4);
    struct blkdev* p1 = blk_find("ram2p1");
    struct blkdev* p2 = blk_find("ram2p2");
    struct blkdev* p5 = blk_find("ram2p5");
    struct blkdev* p6 = blk_find("ram2p6");
    CHECK(p1 && p2 && p5 && p6 && !blk_find("ram2p3") && !blk_find("ram2p4"));
    CHECK(p1->sectors == 512 && p1->start == 64 && p1->part_type == 0x0C);
    CHECK(p5->start == 1312 && p5->sectors == 100 && p5->part_type == 0x0B);
    CHECK(p6->start == 1496 && p6->sectors == 50);

    /* coerencia particao <-> disco inteiro via cache compartilhado */
    uint8_t sec[512], again[512];
    memset(sec, 0x42, sizeof(sec));
    CHECK(blk_write(p1, 3, 1, sec) == 0);
    CHECK(blk_read(disk, 64 + 3, 1, again) == 0 && memcmp(sec, again, 512) == 0);
    CHECK(blk_write(disk, 1024 + 5, 1, sec) == 0);
    CHECK(blk_read(p2, 5, 1, again) == 0 && again[9] == 0x42);
    CHECK(blk_read(p2, 256, 1, again) == -EINVAL);   /* fora da particao */
    CHECK(blk_read(p5, 99, 1, again) == 0);
    CHECK(blk_read(p5, 100, 1, again) == -EINVAL);

    CHECK(blk_unregister(disk) == 0);                /* leva as filhas junto */
    CHECK(!blk_find("ram2p1") && !blk_find("ram2") && backing[(64 + 3) * 512] == 0x42);
    return 0;
}

static int test_gpt(void) {
    static uint8_t backing[4096 * 512];
    memset(backing, 0, sizeof(backing));
    uint8_t* pmbr = backing;
    mbr_entry(pmbr, 0, 0, 0xEE, 1, 4095);
    put16(pmbr + 510, 0xAA55);

    uint8_t* entries = backing + 2 * 512;
    static const uint8_t guid_a[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    memcpy(entries, guid_a, 16);
    put64(entries + 32, 40); put64(entries + 40, 539);          /* 500 setores */
    memcpy(entries + 128, guid_a, 16);
    entries[128] = 0xEE;
    put64(entries + 128 + 32, 1000); put64(entries + 128 + 40, 1999);
    memcpy(entries + 3 * 128, guid_a, 16);                       /* entrada #4 */
    put64(entries + 3 * 128 + 32, 3000); put64(entries + 3 * 128 + 40, 3099);
    memcpy(entries + 4 * 128, guid_a, 16);                       /* #5: fora da area util */
    put64(entries + 4 * 128 + 32, 5000); put64(entries + 4 * 128 + 40, 6000);

    uint8_t* h = backing + 512;
    memcpy(h, "EFI PART", 8);
    put32(h + 8, 0x00010000); put32(h + 12, 92);
    put64(h + 24, 1); put64(h + 32, 4095);
    put64(h + 40, 34); put64(h + 48, 4062);
    put64(h + 72, 2); put32(h + 80, 128); put32(h + 84, 128);
    put32(h + 88, blk_crc32(0, entries, 128 * 128));
    put32(h + 16, blk_crc32(0, h, 92));

    struct blkdev* disk = ramdisk_create("ram3", backing, sizeof(backing), false);
    CHECK(disk);
    CHECK(blk_scan_partitions(disk) == 3);
    struct blkdev* g1 = blk_find("ram3p1");
    struct blkdev* g2 = blk_find("ram3p2");
    struct blkdev* g4 = blk_find("ram3p4");
    CHECK(g1 && g2 && g4 && !blk_find("ram3p3") && !blk_find("ram3p5"));
    CHECK(g1->is_gpt && g1->start == 40 && g1->sectors == 500);
    CHECK(g2->start == 1000 && g2->sectors == 1000 && g2->part_guid[0] == 0xEE);
    CHECK(g4->start == 3000 && g4->sectors == 100);
    CHECK(blk_unregister(disk) == 0);

    /* CRC da tabela corrompido: GPT rejeitada; MBR protetor nao gera nada */
    entries[0] ^= 0x01;
    disk = ramdisk_create("ram3", backing, sizeof(backing), false);
    CHECK(disk && blk_scan_partitions(disk) == 0 && !blk_find("ram3p1"));
    CHECK(blk_unregister(disk) == 0);
    return 0;
}

static int test_superfloppy(void) {
    static uint8_t backing[128 * 512];
    memset(backing, 0, sizeof(backing));
    backing[0] = 0xEB; backing[1] = 0x58; backing[2] = 0x90;
    put16(backing + 11, 512); backing[13] = 8; put16(backing + 14, 32);
    backing[16] = 2;
    mbr_entry(backing, 0, 0x00, 0x0C, 1, 10);       /* lixo que pareceria MBR */
    put16(backing + 510, 0xAA55);
    struct blkdev* disk = ramdisk_create("ram4", backing, sizeof(backing), false);
    CHECK(disk && blk_scan_partitions(disk) == 0 && blk_count() == 1);
    CHECK(blk_unregister(disk) == 0);

    CHECK(blk_scan_partitions(NULL) == -EINVAL);
    struct blkdev* a = ramdisk_alloc("ram5", 64 * 512 + 100);   /* arredonda */
    CHECK(a && a->sectors == 64);
    CHECK(blk_scan_partitions(a) == 0);                          /* sem assinatura */
    ramdisk_destroy(a);
    CHECK(blk_find("ram5") == NULL);
    return 0;
}

static int test_ata(void) {
    memset(&g_ide, 0, sizeof(g_ide));
    g_sim_present = true;
    g_sim_lba48 = false;
    g_sim_reported = SIM_SECTORS;
    memset(g_sim_disk, 0, sizeof(g_sim_disk));
    /* MBR com uma particao no disco simulado */
    mbr_entry(g_sim_disk, 0, 0x80, 0x0C, 128, 1024);
    put16(g_sim_disk + 510, 0xAA55);
    for (int i = 0; i < 512; i++) g_sim_disk[129 * 512 + i] = (uint8_t)(i + 1);

    CHECK(ata_init() == 1);
    struct blkdev* hda = blk_find("hda");
    CHECK(hda && hda->sectors == SIM_SECTORS && !blk_find("hdb") && !blk_find("hdc"));
    struct blkdev* hda1 = blk_find("hda1");
    CHECK(hda1 && hda1->start == 128 && hda1->sectors == 1024);

    uint8_t sec[512];
    CHECK(blk_read(hda1, 1, 1, sec) == 0 && sec[0] == 1 && sec[255] == 0);
    /* leitura multi-setor grande atravessando o limite de 128/cmd */
    static uint8_t big[300 * 512];
    for (int i = 0; i < 300 * 512; i++) g_sim_disk[1000 * 512 + i] = (uint8_t)(i * 13);
    CHECK(blk_read(hda, 1000, 300, big) == 0);
    CHECK(memcmp(big, g_sim_disk + 1000 * 512, sizeof(big)) == 0);

    /* escrita: write-back + flush manda dados e FLUSH ao disco */
    memset(sec, 0xE1, sizeof(sec));
    CHECK(blk_write(hda1, 7, 1, sec) == 0);
    CHECK(g_sim_disk[(128 + 7) * 512] != 0xE1);
    unsigned flushes = g_sim_flushes;
    CHECK(blk_flush(hda) == 0);
    CHECK(g_sim_disk[(128 + 7) * 512] == 0xE1 && g_sim_flushes == flushes + 1);

    /* API crua do ata.h */
    ata_device_t* raw = NULL;
    (void)raw;
    ata_device_t outsider = {.io_base = 0x1F0, .device = 0};
    CHECK(ata_read_sectors(&outsider, 0, sec, 1) == -EINVAL);   /* nao e do driver */

    /* remover o disco (e as particoes) devolve os dados ao hardware */
    memset(sec, 0x99, sizeof(sec));
    CHECK(blk_write(hda1, 8, 1, sec) == 0);
    CHECK(blk_unregister(hda) == 0 && g_sim_disk[(128 + 8) * 512] == 0x99);
    CHECK(blk_find("hda") == NULL && blk_find("hda1") == NULL);
    return 0;
}

static int test_ata_lba48(void) {
    memset(&g_ide, 0, sizeof(g_ide));
    g_sim_present = true;
    g_sim_lba48 = true;
    g_sim_reported = 0x180000000ull;               /* ~3 TiB: precisa de LBA48 */
    memset(g_sim_disk, 0, sizeof(g_sim_disk));
    CHECK(ata_init() == 1);
    struct blkdev* hda = blk_find("hda");
    CHECK(hda && hda->sectors == 0x180000000ull);

    uint8_t sec[512], back[512];
    memset(sec, 0x3C, sizeof(sec));
    uint64_t high = 0x150000007ull;                /* > 2^32: HOB bytes usados */
    CHECK(blk_write(hda, high, 1, sec) == 0);
    CHECK(blk_flush(hda) == 0);
    CHECK(g_sim_last_lba == high);
    CHECK(g_sim_disk[sim_index(high) * 512] == 0x3C);
    /* evita servir do cache: descarta e le de novo do "hardware" */
    CHECK(blk_unregister(hda) == 0);
    CHECK(ata_init() == 1);
    hda = blk_find("hda");
    CHECK(hda && blk_read(hda, high, 1, back) == 0 && g_sim_last_lba == high);
    CHECK(memcmp(sec, back, 512) == 0);
    CHECK(blk_read(hda, 0x180000000ull, 1, back) == -EINVAL);
    CHECK(blk_unregister(hda) == 0);

    /* sem dispositivo: nada registrado */
    g_sim_present = false;
    CHECK(ata_init() == 0 && blk_find("hda") == NULL);
    return 0;
}

int main(void) {
    kheap_init(g_arena, sizeof(g_arena));
    blk_init();
    int r;
    if ((r = test_basics())) return r;
    if ((r = test_cache_pressure())) return r;
    if ((r = test_mbr())) return r;
    if ((r = test_gpt())) return r;
    if ((r = test_superfloppy())) return r;
    if ((r = test_ata())) return r;
    if ((r = test_ata_lba48())) return r;
    return 0;
}
