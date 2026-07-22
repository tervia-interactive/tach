/* tach Operating System - AHCI Header */
#ifndef _DRIVERS_AHCI_H
#define _DRIVERS_AHCI_H

#include <kernel/types.h>
#include <stdint.h>

/* AHCI port types */
#define AHCI_PORT_NONE 0
#define AHCI_PORT_SATA 1
#define AHCI_PORT_PM 2
#define AHCI_PORT_SATAPI 3

/* AHCI HBA structure (simplified) */
typedef struct {
    void* mmio_base;
    uint32_t port_map;
    uint8_t num_ports;
} ahci_hba_t;

/* AHCI port structure */
typedef struct {
    ahci_hba_t* hba;
    uint8_t port_num;
    uint8_t type;
    bool present;
} ahci_port_t;

/* Initialize AHCI controller */
int ahci_init(void);

/* Read sectors from AHCI port */
int ahci_read_sector(ahci_port_t* port, uint32_t lba, uint8_t* buffer);

/* Write sectors to AHCI port */
int ahci_write_sector(ahci_port_t* port, uint32_t lba, const uint8_t* buffer);

#endif /* _DRIVERS_AHCI_H */
