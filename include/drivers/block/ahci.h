#ifndef DRIVERS_BLOCK_AHCI_H
#define DRIVERS_BLOCK_AHCI_H

#include <kernel/types.h>

#define AHCI_SECTOR_SIZE 512
#define AHCI_MAX_PORTS 32

typedef struct {
    uint8_t present;
    uint8_t type;
    uint32_t sectors;
    char model[41];
    char serial[21];
} ahci_port_t;

int ahci_init(void);
int ahci_read_sector(uint8_t port, uint32_t lba, void *buffer);
int ahci_write_sector(uint8_t port, uint32_t lba, const void *buffer);
ahci_port_t *ahci_get_port(uint8_t port);

#endif /* DRIVERS_BLOCK_AHCI_H */
