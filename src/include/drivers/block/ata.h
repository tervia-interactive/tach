/* tach - ATA Header */
#ifndef _DRIVERS_ATA_H
#define _DRIVERS_ATA_H

#include <kernel/types.h>
#include <stdint.h>

/* ATA device types */
#define ATA_DEVICE_MASTER 0
#define ATA_DEVICE_SLAVE 1

/* ATA commands */
#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY 0xEC

/* ATA device structure */
typedef struct {
    uint16_t io_base;
    uint8_t device;
    bool present;
    uint32_t sectors;
} ata_device_t;

/* Initialize ATA controller */
int ata_init(void);

/* Identify ATA device */
int ata_identify(ata_device_t* dev);

/* Read sectors from ATA device */
int ata_read_sectors(ata_device_t* dev, uint32_t lba, uint8_t* buffer, uint8_t count);

/* Write sectors to ATA device */
int ata_write_sectors(ata_device_t* dev, uint32_t lba, const uint8_t* buffer, uint8_t count);

#endif /* _DRIVERS_ATA_H */
