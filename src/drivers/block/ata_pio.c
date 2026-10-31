/*
 * drivers/block/ata_pio.c - ATA PIO driver (LBA28 + LBA48, polling)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 *
 * Sondagem dos dois canais legados (0x1F0/0x170), master e slave. Registra
 * cada disco ATA como "hda".."hdd" na camada de bloco e escaneia particoes.
 * Dispositivos ATAPI/SATA em modo nativo (assinatura LBA1/LBA2 != 0) sao
 * ignorados. Somente x86: nas demais arquiteturas hal_port_* nao existe e a
 * sondagem encontra "nenhum dispositivo".
 */
#include <drivers/block/ata.h>
#include <drivers/block/blk.h>
#include <hal/io.h>
#include <kernel/errno.h>
#include <kernel/klog.h>
#include <kernel/string.h>

#define ATA_REG_DATA   0
#define ATA_REG_FEAT   1
#define ATA_REG_COUNT  2
#define ATA_REG_LBA0   3
#define ATA_REG_LBA1   4
#define ATA_REG_LBA2   5
#define ATA_REG_DRIVE  6
#define ATA_REG_STATUS 7
#define ATA_REG_CMD    7

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF  0x20
#define ATA_SR_BSY 0x80

#define ATA_CMD_READ_EXT  0x24
#define ATA_CMD_WRITE_EXT 0x34
#define ATA_CMD_FLUSH     0xE7
#define ATA_CMD_FLUSH_EXT 0xEA

#define ATA_SPINS 2000000u      /* iteracoes de polling antes do timeout */
#define ATA_CHUNK 128u          /* setores por comando (<= 256) */

struct ata_drive {
    ata_device_t dev;           /* io_base, device, present, sectors (32 bits) */
    uint64_t sectors;           /* tamanho real (LBA48) */
    bool lba48;
    struct blkdev blk;
};

static struct ata_drive g_drives[4];

/* O ata.h expoe apenas ata_device_t; so dispositivos que o proprio driver
 * possui (g_drives) carregam LBA48/tamanho real. */
static struct ata_drive* drive_of(const ata_device_t* dev) {
    for (size_t i = 0; i < 4; i++)
        if (dev == &g_drives[i].dev) return &g_drives[i];
    return NULL;
}

static uint16_t ctrl_of(const ata_device_t* dev) {
    return (uint16_t)(dev->io_base + 0x206);   /* 0x1F0->0x3F6, 0x170->0x376 */
}

static void ata_delay(const ata_device_t* dev) {
    for (int i = 0; i < 4; i++) (void)hal_port_in(ctrl_of(dev));   /* ~400ns */
}

static int wait_not_busy(const ata_device_t* dev) {
    for (uint32_t i = 0; i < ATA_SPINS; i++) {
        uint8_t st = hal_port_in(ctrl_of(dev));        /* status alternativo */
        if (!(st & ATA_SR_BSY)) return st;
    }
    return -ETIMEOUT;
}

static int wait_drq(const ata_device_t* dev) {
    for (uint32_t i = 0; i < ATA_SPINS; i++) {
        uint8_t st = hal_port_in(ctrl_of(dev));
        if (st & ATA_SR_BSY) continue;
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return -EIO;
        if (st & ATA_SR_DRQ) return 0;
    }
    return -ETIMEOUT;
}

static void select_drive(const ata_device_t* dev, uint8_t lba_top) {
    hal_port_out((uint16_t)(dev->io_base + ATA_REG_DRIVE),
                 (uint8_t)(0xE0 | (dev->device << 4) | (lba_top & 0x0F)));
    ata_delay(dev);
}

int ata_identify(ata_device_t* dev) {
    if (!dev) return -EINVAL;
    uint16_t io = dev->io_base;
    dev->present = false;
    dev->sectors = 0;

    hal_port_out((uint16_t)(io + ATA_REG_DRIVE),
                 (uint8_t)(0xA0 | (dev->device << 4)));
    ata_delay(dev);
    hal_port_out((uint16_t)(io + ATA_REG_COUNT), 0);
    hal_port_out((uint16_t)(io + ATA_REG_LBA0), 0);
    hal_port_out((uint16_t)(io + ATA_REG_LBA1), 0);
    hal_port_out((uint16_t)(io + ATA_REG_LBA2), 0);
    hal_port_out((uint16_t)(io + ATA_REG_CMD), ATA_CMD_IDENTIFY);

    uint8_t st = hal_port_in((uint16_t)(io + ATA_REG_STATUS));
    if (st == 0x00 || st == 0xFF) return -ENODEV;   /* sem dispositivo/barramento flutuando */
    int wait = wait_not_busy(dev);
    if (wait < 0) return wait;
    if (hal_port_in((uint16_t)(io + ATA_REG_LBA1)) ||
        hal_port_in((uint16_t)(io + ATA_REG_LBA2)))
        return -ENODEV;                              /* ATAPI / SATA */
    wait = wait_drq(dev);
    if (wait < 0) return wait;

    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = hal_port_in16((uint16_t)(io + ATA_REG_DATA));

    uint64_t total = (uint64_t)id[60] | (uint64_t)id[61] << 16;
    bool lba48 = (id[83] & (1u << 10)) != 0;
    if (lba48) {
        uint64_t big = (uint64_t)id[100] | (uint64_t)id[101] << 16 |
                       (uint64_t)id[102] << 32 | (uint64_t)id[103] << 48;
        if (big) total = big;
    }
    if (!total) return -ENODEV;

    struct ata_drive* drive = drive_of(dev);
    if (drive) {
        drive->sectors = total;
        drive->lba48 = lba48;
    } else if (total > 0x0FFFFFFFull) {
        total = 0x0FFFFFFFull;      /* handle externo: so enderecamento LBA28 */
    }
    dev->sectors = total > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)total;
    dev->present = true;
    return 0;
}

/* Programa e dispara um comando de leitura/escrita. */
static int issue(struct ata_drive* drive, uint64_t lba, uint32_t count,
                 bool write) {
    ata_device_t* dev = &drive->dev;
    uint16_t io = dev->io_base;
    bool ext = drive->lba48 && (lba + count > 0x0FFFFFFFull || count > 256);
    if (!ext && lba + count > 0x0FFFFFFFull) return -EINVAL;

    int wait = wait_not_busy(dev);
    if (wait < 0) return wait;
    if (ext) {
        select_drive(dev, 0);
        hal_port_out((uint16_t)(io + ATA_REG_COUNT), (uint8_t)(count >> 8));
        hal_port_out((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba >> 24));
        hal_port_out((uint16_t)(io + ATA_REG_LBA1), (uint8_t)(lba >> 32));
        hal_port_out((uint16_t)(io + ATA_REG_LBA2), (uint8_t)(lba >> 40));
        hal_port_out((uint16_t)(io + ATA_REG_COUNT), (uint8_t)count);
        hal_port_out((uint16_t)(io + ATA_REG_LBA0), (uint8_t)lba);
        hal_port_out((uint16_t)(io + ATA_REG_LBA1), (uint8_t)(lba >> 8));
        hal_port_out((uint16_t)(io + ATA_REG_LBA2), (uint8_t)(lba >> 16));
        hal_port_out((uint16_t)(io + ATA_REG_CMD),
                     write ? ATA_CMD_WRITE_EXT : ATA_CMD_READ_EXT);
    } else {
        select_drive(dev, (uint8_t)(lba >> 24));
        hal_port_out((uint16_t)(io + ATA_REG_COUNT), (uint8_t)count);  /* 256 -> 0 */
        hal_port_out((uint16_t)(io + ATA_REG_LBA0), (uint8_t)lba);
        hal_port_out((uint16_t)(io + ATA_REG_LBA1), (uint8_t)(lba >> 8));
        hal_port_out((uint16_t)(io + ATA_REG_LBA2), (uint8_t)(lba >> 16));
        hal_port_out((uint16_t)(io + ATA_REG_CMD),
                     write ? ATA_CMD_WRITE_SECTORS : ATA_CMD_READ_SECTORS);
    }
    ata_delay(dev);                 /* 400ns antes de ler o status */
    return 0;
}

static int pio_read(struct ata_drive* drive, uint64_t lba, uint32_t count,
                    uint8_t* buf) {
    while (count) {
        uint32_t n = count > ATA_CHUNK ? ATA_CHUNK : count;
        int result = issue(drive, lba, n, false);
        if (result < 0) return result;
        for (uint32_t s = 0; s < n; s++) {
            result = wait_drq(&drive->dev);
            if (result < 0) return result;
            uint16_t* words = (uint16_t*)(void*)buf;
            for (int i = 0; i < 256; i++)
                words[i] = hal_port_in16(
                    (uint16_t)(drive->dev.io_base + ATA_REG_DATA));
            buf += BLK_SECTOR_SIZE;
        }
        lba += n;
        count -= n;
    }
    return 0;
}

static int pio_flush(struct ata_drive* drive) {
    int wait = wait_not_busy(&drive->dev);
    if (wait < 0) return wait;
    select_drive(&drive->dev, 0);
    hal_port_out((uint16_t)(drive->dev.io_base + ATA_REG_CMD),
                 drive->lba48 ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);
    wait = wait_not_busy(&drive->dev);
    if (wait < 0) return wait;
    return (wait & (ATA_SR_ERR | ATA_SR_DF)) ? -EIO : 0;
}

static int pio_write(struct ata_drive* drive, uint64_t lba, uint32_t count,
                     const uint8_t* buf) {
    while (count) {
        uint32_t n = count > ATA_CHUNK ? ATA_CHUNK : count;
        int result = issue(drive, lba, n, true);
        if (result < 0) return result;
        for (uint32_t s = 0; s < n; s++) {
            result = wait_drq(&drive->dev);
            if (result < 0) return result;
            const uint16_t* words = (const uint16_t*)(const void*)buf;
            for (int i = 0; i < 256; i++)
                hal_port_out16((uint16_t)(drive->dev.io_base + ATA_REG_DATA),
                               words[i]);
            buf += BLK_SECTOR_SIZE;
        }
        /* Espera o disco terminar de gravar o ultimo setor deste comando. */
        int status = wait_not_busy(&drive->dev);
        if (status < 0) return status;
        if (status & (ATA_SR_ERR | ATA_SR_DF)) return -EIO;
        lba += n;
        count -= n;
    }
    return 0;
}

/* ------------------------------------------------- API do ata.h (32 bits) */

int ata_read_sectors(ata_device_t* dev, uint32_t lba, uint8_t* buffer,
                     uint8_t count) {
    struct ata_drive* drive = drive_of(dev);
    if (!drive || !dev->present || !buffer || !count) return -EINVAL;
    return pio_read(drive, lba, count, buffer);
}

int ata_write_sectors(ata_device_t* dev, uint32_t lba, const uint8_t* buffer,
                      uint8_t count) {
    struct ata_drive* drive = drive_of(dev);
    if (!drive || !dev->present || !buffer || !count) return -EINVAL;
    return pio_write(drive, lba, count, buffer);
}

/* ------------------------------------------------------- ponte para blk */

static int blk_ata_read(struct blkdev* b, uint64_t lba, uint32_t count,
                        void* buf) {
    return pio_read(b->priv, lba, count, buf);
}

static int blk_ata_write(struct blkdev* b, uint64_t lba, uint32_t count,
                         const void* buf) {
    return pio_write(b->priv, lba, count, buf);
}

static int blk_ata_flush(struct blkdev* b) { return pio_flush(b->priv); }

static const struct blkdev_ops g_ata_ops = {blk_ata_read, blk_ata_write,
                                            blk_ata_flush};

int ata_init(void) {
    static const uint16_t bases[2] = {0x1F0, 0x170};
    int found = 0;
    blk_init();
    for (int channel = 0; channel < 2; channel++) {
        /* nIEN=1: sem IRQ14/15; o driver usa somente polling. */
        ata_device_t probe = {.io_base = bases[channel]};
        hal_port_out(ctrl_of(&probe), 0x02);
        for (int unit = 0; unit < 2; unit++) {
            int slot = channel * 2 + unit;
            struct ata_drive* drive = &g_drives[slot];
            memset(drive, 0, sizeof(*drive));
            drive->dev.io_base = bases[channel];
            drive->dev.device = (uint8_t)unit;
            if (ata_identify(&drive->dev) < 0) continue;

            drive->blk.name[0] = 'h';
            drive->blk.name[1] = 'd';
            drive->blk.name[2] = (char)('a' + slot);
            drive->blk.name[3] = '\0';
            drive->blk.sectors = drive->sectors;
            drive->blk.ops = &g_ata_ops;
            drive->blk.priv = drive;
            if (blk_register(&drive->blk) < 0) continue;
            klog_info("ata", "%s: %u MiB%s", drive->blk.name,
                      (unsigned)(drive->sectors / 2048),
                      drive->lba48 ? " (LBA48)" : "");
            int parts = blk_scan_partitions(&drive->blk);
            if (parts > 0)
                klog_info("ata", "%s: %d partition(s)", drive->blk.name, parts);
            found++;
        }
    }
    return found;
}

void ata_pio_init(void) { (void)ata_init(); }
