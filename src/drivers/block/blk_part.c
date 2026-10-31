/*
 * drivers/block/blk_part.c - MBR (primary + extended) and GPT discovery
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#include <drivers/block/blk.h>
#include <kernel/errno.h>
#include <kernel/klog.h>
#include <kernel/string.h>
#include <mm/kheap.h>

#define MBR_TABLE_OFFSET 446
#define MBR_ENTRY_SIZE   16
#define MBR_EXT_MAX      32     /* limite de EBRs encadeados */
#define GPT_MAX_ENTRIES  1024

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
           (uint32_t)p[3] << 24;
}
static uint64_t rd64(const uint8_t* p) {
    return (uint64_t)rd32(p) | (uint64_t)rd32(p + 4) << 32;
}

uint32_t blk_crc32(uint32_t crc, const void* data, size_t len) {
    const uint8_t* p = data;
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1));
    }
    return ~crc;
}

/* Nome: "hda"+1 -> "hda1"; se o disco termina em digito, "ram0"+1 -> "ram0p1". */
static void part_name(char* out, const char* disk, unsigned number) {
    size_t n = strlen(disk);
    char digits[12];
    size_t d = 0;
    unsigned v = number;
    do { digits[d++] = (char)('0' + v % 10); v /= 10; } while (v && d < 11);
    bool sep = n && disk[n - 1] >= '0' && disk[n - 1] <= '9';
    size_t pos = 0;
    for (size_t i = 0; i < n && pos < BLK_NAME_MAX - 1; i++) out[pos++] = disk[i];
    if (sep && pos < BLK_NAME_MAX - 1) out[pos++] = 'p';
    while (d && pos < BLK_NAME_MAX - 1) out[pos++] = digits[--d];
    out[pos] = '\0';
}

static int add_partition(struct blkdev* disk, unsigned number, uint64_t start,
                         uint64_t sectors, uint8_t type, const uint8_t* guid) {
    if (!sectors || start > disk->sectors || sectors > disk->sectors - start)
        return -EINVAL;
    struct blkdev* part = kzalloc(sizeof(*part));
    if (!part) return -ENOMEM;
    part_name(part->name, disk->name, number);
    part->sectors = sectors;
    part->readonly = disk->readonly;
    part->parent = disk;
    part->start = start;
    part->part_type = type;
    if (guid) {
        memcpy(part->part_guid, guid, 16);
        part->is_gpt = true;
    }
    part->owned = true;
    int result = blk_register(part);
    if (result < 0) {
        kfree(part);
        return result;
    }
    klog_info("blk", "%s: %u sectors @ %u (type 0x%x)", part->name,
              (unsigned)sectors, (unsigned)start, (unsigned)type);
    return 0;
}

/* Boot sector FAT sem tabela de particoes (disquete / pendrive formatado
 * direto). Evita confundir o jmp+BPB com um MBR. */
static bool looks_like_fat_vbr(const uint8_t* s) {
    if (s[0] != 0xEB && s[0] != 0xE9) return false;
    uint16_t bps = rd16(s + 11);
    uint8_t spc = s[13];
    uint8_t fats = s[16];
    if (bps != 512 || !spc || (spc & (spc - 1))) return false;
    if (fats == 0 || fats > 2) return false;
    return rd16(s + 14) != 0;
}

static int scan_gpt(struct blkdev* disk, int* created) {
    uint8_t h[BLK_SECTOR_SIZE];
    if (blk_read_bytes(disk, BLK_SECTOR_SIZE, h, sizeof(h)) < 0) return -EIO;
    if (memcmp(h, "EFI PART", 8) != 0) return -EINVAL;

    uint32_t header_size = rd32(h + 12);
    uint32_t header_crc = rd32(h + 16);
    if (header_size < 92 || header_size > BLK_SECTOR_SIZE) return -EINVAL;
    memset(h + 16, 0, 4);
    if (blk_crc32(0, h, header_size) != header_crc) return -EINVAL;

    uint64_t first_usable = rd64(h + 40);
    uint64_t last_usable = rd64(h + 48);
    uint64_t entries_lba = rd64(h + 72);
    uint32_t count = rd32(h + 80);
    uint32_t entry_size = rd32(h + 84);
    uint32_t entries_crc = rd32(h + 88);
    if (!count || count > GPT_MAX_ENTRIES) return -EINVAL;
    if (entry_size != 128 && entry_size != 256 && entry_size != 512)
        return -EINVAL;
    if (first_usable > last_usable || last_usable >= disk->sectors)
        return -EINVAL;
    uint64_t table_bytes = (uint64_t)count * entry_size;
    uint64_t table_sectors = (table_bytes + BLK_SECTOR_SIZE - 1) / BLK_SECTOR_SIZE;
    if (entries_lba < 2 || entries_lba > disk->sectors ||
        table_sectors > disk->sectors - entries_lba)
        return -EINVAL;

    /* Passo 1: CRC da tabela inteira. */
    uint8_t sector[BLK_SECTOR_SIZE];
    uint32_t crc = 0;
    uint64_t remaining = table_bytes;
    for (uint64_t i = 0; remaining; i++) {
        size_t chunk = remaining < BLK_SECTOR_SIZE ? (size_t)remaining
                                                   : BLK_SECTOR_SIZE;
        if (blk_read_bytes(disk, (entries_lba + i) * BLK_SECTOR_SIZE, sector,
                           BLK_SECTOR_SIZE) < 0)
            return -EIO;
        crc = blk_crc32(crc, sector, chunk);
        remaining -= chunk;
    }
    if (crc != entries_crc) return -EINVAL;

    /* Passo 2: cria as particoes (entry_size divide 512: sem cruzar setores). */
    uint32_t per_sector = BLK_SECTOR_SIZE / entry_size;
    for (uint32_t index = 0; index < count; index++) {
        if (index % per_sector == 0) {
            if (blk_read_bytes(disk,
                    (entries_lba + index / per_sector) * BLK_SECTOR_SIZE,
                    sector, BLK_SECTOR_SIZE) < 0)
                return -EIO;
        }
        const uint8_t* e = sector + (index % per_sector) * entry_size;
        bool unused = true;
        for (int i = 0; i < 16; i++) if (e[i]) { unused = false; break; }
        if (unused) continue;
        uint64_t first = rd64(e + 32), last = rd64(e + 40);
        if (first < first_usable || last > last_usable || last < first) continue;
        if (add_partition(disk, index + 1, first, last - first + 1, 0, e) == 0)
            (*created)++;
    }
    return 0;
}

static void scan_extended(struct blkdev* disk, uint64_t ext_start,
                          uint64_t ext_sectors, int* created) {
    uint64_t ebr = ext_start;
    unsigned number = 5;
    for (int hops = 0; hops < MBR_EXT_MAX; hops++) {
        uint8_t s[BLK_SECTOR_SIZE];
        if (ebr >= disk->sectors ||
            blk_read_bytes(disk, ebr * BLK_SECTOR_SIZE, s, sizeof(s)) < 0)
            return;
        if (rd16(s + 510) != 0xAA55) return;
        const uint8_t* logical = s + MBR_TABLE_OFFSET;
        const uint8_t* next = logical + MBR_ENTRY_SIZE;
        uint32_t start = rd32(logical + 8), count = rd32(logical + 12);
        if (logical[4] && count &&
            add_partition(disk, number, ebr + start, count, logical[4],
                          NULL) == 0) {
            (*created)++;
            number++;
        }
        uint32_t link = rd32(next + 8);
        uint8_t type = next[4];
        if (!link || (type != 0x05 && type != 0x0F && type != 0x85)) return;
        if (link >= ext_sectors) return;
        ebr = ext_start + link;
    }
}

int blk_scan_partitions(struct blkdev* disk) {
    if (!disk || disk->parent) return -EINVAL;
    uint8_t s[BLK_SECTOR_SIZE];
    if (disk->sectors < 2) return 0;
    int result = blk_read_bytes(disk, 0, s, sizeof(s));
    if (result < 0) return result;
    if (rd16(s + 510) != 0xAA55) return 0;
    if (looks_like_fat_vbr(s)) return 0;

    int created = 0;
    const uint8_t* table = s + MBR_TABLE_OFFSET;
    if (table[4] == 0xEE) {
        if (scan_gpt(disk, &created) == 0) return created;
        created = 0;            /* GPT invalida: cai para o MBR protetor */
    }
    for (int i = 0; i < 4; i++) {
        const uint8_t* e = table + i * MBR_ENTRY_SIZE;
        uint8_t boot = e[0], type = e[4];
        uint32_t start = rd32(e + 8), count = rd32(e + 12);
        if ((boot != 0x00 && boot != 0x80) || !type || !count) continue;
        if (type == 0xEE) continue;
        if (type == 0x05 || type == 0x0F || type == 0x85) {
            scan_extended(disk, start, count, &created);
            continue;
        }
        if (add_partition(disk, (unsigned)i + 1, start, count, type, NULL) == 0)
            created++;
    }
    return created;
}
