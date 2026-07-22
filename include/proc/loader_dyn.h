/*
 * proc/loader_dyn.h - Dynamic ELF (.so) support, for shared framework libs later
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */
#ifndef _PROC_LOADER_DYN_H
#define _PROC_LOADER_DYN_H

#include <kernel/types.h>
#include <proc/elf.h>

struct elf_dynamic_section {
    uint64_t tag;
    uint64_t value;
};

#define DT_NULL     0
#define DT_NEEDED   1
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9
#define DT_JMPREL   23

int elf_load_shared(const void* data, void* base);
int elf_resolve_symbols(struct process* proc, void* base);
void* elf_get_symbol(void* base, const char* name);

#endif /* _PROC_LOADER_DYN_H */
