/*
 * hw/pci.h - PCI/PCIe enumeration and config space
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _HW_PCI_H
#define _HW_PCI_H

#include <kernel/types.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_STATUS 0x06
#define PCI_REVISION_ID 0x08
#define PCI_PROG_IF 0x09
#define PCI_SUBCLASS 0x0A
#define PCI_CLASS 0x0B
#define PCI_HEADER_TYPE 0x0E
#define PCI_BAR0 0x10

#define PCI_CLASS_STORAGE 0x01
#define PCI_CLASS_NETWORK 0x02
#define PCI_CLASS_DISPLAY 0x03
#define PCI_CLASS_USB 0x0C

#define PCI_CLASS_STORAGE_SATA 0x06
#define PCI_CLASS_STORAGE_NVME 0x08

struct pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t header_type;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint32_t bar[6];
};

void pci_init(void);
uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
struct pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id);
void pci_enumerate_devices(void);
int pci_scan(void (*callback)(uint32_t encoded_bdf));
size_t pci_device_count(void);
struct pci_device* pci_device_at(size_t index);

#endif /* _HW_PCI_H */
