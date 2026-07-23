/* tach - ELF Loader Implementation */
#include <kernel/types.h>
#include <proc/elf.h>
#include <proc/process.h>

int elf_validate(const void* data) {
    (void)data;
    return 0;
}

int elf_load(const void* data, struct process* proc) {
    (void)data;
    (void)proc;
    return 0;
}

void* elf_get_entry(const void* data) {
    (void)data;
    return (void*)0;
}
