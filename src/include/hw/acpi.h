/*
 * hw/acpi.h - ACPI table parser (RSDP, MADT, FADT)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _HW_ACPI_H
#define _HW_ACPI_H

#include <kernel/types.h>

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct acpi_madt {
    struct acpi_sdt_header header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed));

#define ACPI_MADT_TYPE_LOCAL_APIC 0
#define ACPI_MADT_TYPE_IO_APIC 1
#define ACPI_MADT_TYPE_INTERRUPT_OVERRIDE 2

struct acpi_madt_entry {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

void acpi_init(void);
struct acpi_sdt_header* acpi_find_table(const char* signature);
void* acpi_get_rsdp(void);
size_t acpi_cpu_count(void);
uint32_t acpi_cpu_apic_id(size_t index);

#endif /* _HW_ACPI_H */
