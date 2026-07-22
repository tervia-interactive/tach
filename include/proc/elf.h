/*
 * proc/elf.h - ELF parser (loads userland binaries)
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_ELF_H
#define _PROC_ELF_H

#include <kernel/types.h>

#define ELF_MAGIC "\x7fELF"
#define ELF_CLASS_32 1
#define ELF_CLASS_64 2
#define ELF_TYPE_EXEC 2
#define ELF_TYPE_DYN 3
#define ELF_MACHINE_X86 3
#define ELF_MACHINE_X86_64 62
#define ELF_MACHINE_ARM 40
#define ELF_MACHINE_AARCH64 183

struct elf_header {
    uint8_t magic[4];
    uint8_t class;
    uint8_t endian;
    uint8_t version;
    uint8_t osabi;
    uint8_t padding[8];
    uint16_t type;
    uint16_t machine;
    uint32_t elf_version;
} __attribute__((packed));

struct elf_program_header {
    uint32_t type;
    uint32_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed));

int elf_validate(const void* data);
int elf_load(const void* data, struct process* proc);
void* elf_get_entry(const void* data);

#endif /* _PROC_ELF_H */
