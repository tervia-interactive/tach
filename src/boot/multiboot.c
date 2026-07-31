#include <boot/multiboot.h>
#include <boot/multiboot1.h>
#include <boot/multiboot2.h>
#include <hal/mm.h>

static uint32_t normalize_type(uint32_t type) {
    switch (type) {
        case 1: return MEM_REGION_USABLE;
        case 3: return MEM_REGION_ACPI;
        case 4: return MEM_REGION_NVS;
        default: return MEM_REGION_RESERVED;
    }
}

static int parse_multiboot1(uintptr_t address) {
    const struct multiboot1_info* info =
        (const struct multiboot1_info*)address;
    int regions = 0;

    hal_memmap_add(address, sizeof(*info), MEM_REGION_RESERVED);
    if (info->flags & MULTIBOOT1_INFO_MMAP) {
        uintptr_t cursor = (uintptr_t)info->mmap_addr;
        uintptr_t end = cursor + info->mmap_length;
        hal_memmap_add(cursor, info->mmap_length, MEM_REGION_RESERVED);
        while (cursor + sizeof(uint32_t) <= end) {
            const struct multiboot1_mmap_entry* entry =
                (const struct multiboot1_mmap_entry*)cursor;
            size_t record_size = (size_t)entry->size + sizeof(entry->size);
            if (record_size < sizeof(*entry) || cursor + record_size > end) {
                return -1;
            }
            if (entry->len &&
                hal_memmap_add(entry->addr, entry->len,
                               normalize_type(entry->type)) == 0) {
                regions++;
            }
            cursor += record_size;
        }
    } else if (info->flags & MULTIBOOT1_INFO_MEMORY) {
        uint64_t upper_end = 0x100000ULL +
                             (uint64_t)info->mem_upper * 1024ULL;
        hal_memmap_add(0, 0x100000, MEM_REGION_RESERVED);
        if (upper_end > 0x100000 &&
            hal_memmap_add(0x100000, upper_end - 0x100000,
                           MEM_REGION_USABLE) == 0) {
            regions++;
        }
    }

    if (info->flags & MULTIBOOT1_INFO_MODULES) {
        const struct multiboot1_module* modules =
            (const struct multiboot1_module*)(uintptr_t)info->mods_addr;
        size_t bytes = (size_t)info->mods_count * sizeof(*modules);
        hal_memmap_add((uintptr_t)modules, bytes, MEM_REGION_RESERVED);
        for (uint32_t i = 0; i < info->mods_count; i++) {
            if (modules[i].mod_end > modules[i].mod_start) {
                hal_memmap_add(modules[i].mod_start,
                               modules[i].mod_end - modules[i].mod_start,
                               MEM_REGION_RESERVED);
            }
        }
    }
    return regions;
}

static int parse_multiboot2(uintptr_t address) {
    const uint32_t* header = (const uint32_t*)address;
    uint32_t total_size = header[0];
    if (total_size < 16) {
        return -1;
    }

    uintptr_t end = address + total_size;
    uintptr_t cursor = address + 8;
    int regions = 0;
    hal_memmap_add(address, total_size, MEM_REGION_RESERVED);

    while (cursor + sizeof(struct multiboot2_tag) <= end) {
        const struct multiboot2_tag* tag =
            (const struct multiboot2_tag*)cursor;
        if (tag->size < sizeof(*tag) || cursor + tag->size > end) {
            return -1;
        }
        if (tag->type == MULTIBOOT2_TAG_TYPE_END) {
            break;
        }
        if (tag->type == MULTIBOOT2_TAG_TYPE_MMAP) {
            const struct multiboot2_tag_mmap* mmap =
                (const struct multiboot2_tag_mmap*)tag;
            if (mmap->entry_size < sizeof(struct multiboot2_mmap_entry)) {
                return -1;
            }
            uintptr_t entry_cursor = (uintptr_t)mmap->entries;
            uintptr_t entries_end = cursor + tag->size;
            while (entry_cursor + mmap->entry_size <= entries_end) {
                const struct multiboot2_mmap_entry* entry =
                    (const struct multiboot2_mmap_entry*)entry_cursor;
                if (entry->len &&
                    hal_memmap_add(entry->addr, entry->len,
                                   normalize_type(entry->type)) == 0) {
                    regions++;
                }
                entry_cursor += mmap->entry_size;
            }
        } else if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
            const struct multiboot2_tag_module* module =
                (const struct multiboot2_tag_module*)tag;
            if (module->mod_end > module->mod_start) {
                hal_memmap_add(module->mod_start,
                               module->mod_end - module->mod_start,
                               MEM_REGION_RESERVED);
            }
        }
        cursor = (cursor + tag->size + 7u) & ~(uintptr_t)7u;
    }
    return regions;
}

int multiboot_parse(uint32_t magic, uintptr_t info) {
    hal_memmap_reset();
    if (!info) {
        return -1;
    }
    if (magic == MULTIBOOT1_BOOTLOADER_MAGIC) {
        return parse_multiboot1(info);
    }
    if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        return parse_multiboot2(info);
    }
    return -1;
}
