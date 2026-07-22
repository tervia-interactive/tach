/*
 * boot/uefi.h - UEFI boot structures and protocols
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _BOOT_UEFI_H
#define _BOOT_UEFI_H

#include <kernel/types.h>

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid_t;

typedef void* efi_handle_t;
typedef void* efi_event_t;

typedef struct {
    efi_guid_t guid;
    void* table;
} efi_configuration_table_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
    uint64_t firmware_vendor;
    uint64_t firmware_revision;
    uint64_t console_in_handle;
    uint64_t console_out_handle;
    uint64_t standard_error_handle;
    uint64_t runtime_services;
    uint64_t boot_services;
    uint32_t number_of_table_entries;
    uint32_t padding;
    efi_configuration_table_t configuration_table[];
} efi_system_table_t;

#define EFI_MEMORY_MAP_TYPE_USABLE 1
#define EFI_MEMORY_MAP_TYPE_RESERVED 2
#define EFI_MEMORY_MAP_TYPE_ACPI_RECLAIMABLE 3
#define EFI_MEMORY_MAP_TYPE_ACPI_NVS 4
#define EFI_MEMORY_MAP_TYPE_BAD 5

typedef struct {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

#endif /* _BOOT_UEFI_H */
