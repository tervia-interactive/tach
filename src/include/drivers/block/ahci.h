#ifndef _DRIVERS_AHCI_H
#define _DRIVERS_AHCI_H

#include <kernel/types.h>
#include <drivers/block/block.h>
#include <kernel/spinlock.h>

#define AHCI_PORT_NONE 0
#define AHCI_PORT_SATA 1
#define AHCI_PORT_PM 2
#define AHCI_PORT_SATAPI 3
#define AHCI_MAX_PORTS 32

typedef struct {
    volatile void* mmio_base;
    uint32_t port_map;
    uint8_t num_ports;
} ahci_hba_t;

typedef struct {
    ahci_hba_t* hba;
    uint8_t port_num;
    uint8_t type;
    bool present;
    volatile void* registers;
    void* command_list;
    void* received_fis;
    void* command_tables;
    void* bounce;
    spinlock_t lock;
    struct block_device block;
} ahci_port_t;

int ahci_init(void);
size_t ahci_port_count(void);
ahci_port_t* ahci_port_at(size_t index);
int ahci_read(ahci_port_t* port, uint64_t lba, uint32_t count, void* buffer);
int ahci_write(ahci_port_t* port, uint64_t lba, uint32_t count,
               const void* buffer);
int ahci_read_sector(ahci_port_t* port, uint32_t lba, uint8_t* buffer);
int ahci_write_sector(ahci_port_t* port, uint32_t lba,
                      const uint8_t* buffer);

#endif
