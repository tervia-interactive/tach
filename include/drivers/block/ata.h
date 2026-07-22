#ifndef DRIVERS_BLOCK_ATA_H
#define DRIVERS_BLOCK_ATA_H

#include <kernel/types.h>

#define ATA_SECTOR_SIZE 512
#define ATA_MAX_DRIVES 4

typedef enum {
    ATA_DRIVE_MASTER = 0,
    ATA_DRIVE_SLAVE = 1,
    ATA_DRIVE_MASTER_2 = 2,
    ATA_DRIVE_SLAVE_2 = 3
} ata_drive_t;

typedef struct {
    uint8_t present;
    uint8_t type;
    uint32_t sectors;
    char model[41];
    char serial[21];
} ata_device_t;

int ata_init(void);
int ata_read_sector(ata_drive_t drive, uint32_t lba, void *buffer);
int ata_write_sector(ata_drive_t drive, uint32_t lba, const void *buffer);
ata_device_t *ata_get_device(ata_drive_t drive);

#endif /* DRIVERS_BLOCK_ATA_H */
