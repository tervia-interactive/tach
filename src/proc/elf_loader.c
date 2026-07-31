#include <kernel/errno.h>
#include <kernel/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/elf.h>

#define ELF_MAX_PROGRAM_HEADERS 128
#define ELF_MAX_LOADED_PAGES 512

#if UINTPTR_MAX > UINT32_MAX
#define ELF_DYN_BASE 0x0000001000000000ULL
#else
#define ELF_DYN_BASE 0x10000000UL
#endif

struct parsed_elf {
    uint8_t elf_class;
    uint16_t type;
    uint16_t machine;
    uint64_t entry;
    uint64_t phoff;
    uint16_t phentsize;
    uint16_t phnum;
};

struct parsed_segment {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t filesz;
    uint64_t memsz;
};

static bool range_fits(uint64_t offset, uint64_t length, size_t size) {
    return offset <= size && length <= (uint64_t)size - offset;
}

static bool machine_matches(uint16_t machine) {
#if defined(__x86_64__)
    return machine == ELF_MACHINE_X86_64;
#elif defined(__i386__)
    return machine == ELF_MACHINE_X86;
#elif defined(__aarch64__)
    return machine == ELF_MACHINE_AARCH64;
#elif defined(__arm__)
    return machine == ELF_MACHINE_ARM;
#elif defined(__riscv)
    return machine == ELF_MACHINE_RISCV;
#else
    return machine != 0;
#endif
}

static int parse_header(const void* data, size_t size,
                        struct parsed_elf* parsed) {
    if (!data || !parsed || size < 16) return -ENOEXEC;
    const uint8_t* ident = (const uint8_t*)data;
    if (memcmp(ident, ELF_MAGIC, 4) != 0 ||
        ident[5] != ELF_DATA_LSB ||
        ident[6] != ELF_VERSION_CURRENT) {
        return -ENOEXEC;
    }

    memset(parsed, 0, sizeof(*parsed));
    parsed->elf_class = ident[4];
    if (ident[4] == ELF_CLASS_32) {
        if (size < sizeof(struct elf32_header)) return -ENOEXEC;
        const struct elf32_header* header =
            (const struct elf32_header*)data;
        parsed->type = header->type;
        parsed->machine = header->machine;
        parsed->entry = header->entry;
        parsed->phoff = header->phoff;
        parsed->phentsize = header->phentsize;
        parsed->phnum = header->phnum;
        if (header->version != ELF_VERSION_CURRENT ||
            header->ehsize < sizeof(*header) ||
            header->phentsize < sizeof(struct elf32_program_header)) {
            return -ENOEXEC;
        }
    } else if (ident[4] == ELF_CLASS_64) {
        if (size < sizeof(struct elf64_header)) return -ENOEXEC;
        const struct elf64_header* header =
            (const struct elf64_header*)data;
        parsed->type = header->type;
        parsed->machine = header->machine;
        parsed->entry = header->entry;
        parsed->phoff = header->phoff;
        parsed->phentsize = header->phentsize;
        parsed->phnum = header->phnum;
        if (header->version != ELF_VERSION_CURRENT ||
            header->ehsize < sizeof(*header) ||
            header->phentsize < sizeof(struct elf64_program_header)) {
            return -ENOEXEC;
        }
    } else {
        return -ENOEXEC;
    }

#if UINTPTR_MAX > UINT32_MAX
    if (parsed->elf_class != ELF_CLASS_64) return -ENOEXEC;
#else
    if (parsed->elf_class != ELF_CLASS_32) return -ENOEXEC;
#endif
    if ((parsed->type != ELF_TYPE_EXEC && parsed->type != ELF_TYPE_DYN) ||
        !machine_matches(parsed->machine) ||
        !parsed->phnum || parsed->phnum > ELF_MAX_PROGRAM_HEADERS ||
        !range_fits(parsed->phoff,
                    (uint64_t)parsed->phentsize * parsed->phnum, size)) {
        return -ENOEXEC;
    }
    return 0;
}

static int parse_segment(const uint8_t* image, size_t size,
                         const struct parsed_elf* elf, size_t index,
                         struct parsed_segment* segment) {
    uint64_t offset = elf->phoff + (uint64_t)index * elf->phentsize;
    if (!range_fits(offset, elf->phentsize, size)) return -ENOEXEC;
    memset(segment, 0, sizeof(*segment));
    if (elf->elf_class == ELF_CLASS_32) {
        const struct elf32_program_header* header =
            (const struct elf32_program_header*)(image + offset);
        segment->type = header->type;
        segment->flags = header->flags;
        segment->offset = header->offset;
        segment->vaddr = header->vaddr;
        segment->filesz = header->filesz;
        segment->memsz = header->memsz;
    } else {
        const struct elf64_program_header* header =
            (const struct elf64_program_header*)(image + offset);
        segment->type = header->type;
        segment->flags = header->flags;
        segment->offset = header->offset;
        segment->vaddr = header->vaddr;
        segment->filesz = header->filesz;
        segment->memsz = header->memsz;
    }
    if (segment->type == ELF_PT_LOAD &&
        (segment->filesz > segment->memsz ||
         !range_fits(segment->offset, segment->filesz, size) ||
         segment->vaddr + segment->memsz < segment->vaddr)) {
        return -ENOEXEC;
    }
    return 0;
}

int elf_validate(const void* data, size_t size) {
    struct parsed_elf elf;
    int result = parse_header(data, size, &elf);
    if (result < 0) return result;
    bool loadable = false;
    for (size_t i = 0; i < elf.phnum; i++) {
        struct parsed_segment segment;
        result = parse_segment((const uint8_t*)data, size, &elf, i, &segment);
        if (result < 0) return result;
        if (segment.type == ELF_PT_LOAD && segment.memsz) loadable = true;
    }
    return loadable ? 0 : -ENOEXEC;
}

int elf_load(const void* data, size_t size, struct process* proc) {
    if (!proc || !proc->mm) return -EINVAL;
    struct parsed_elf elf;
    int result = parse_header(data, size, &elf);
    if (result < 0) return result;
    uintptr_t bias = elf.type == ELF_TYPE_DYN ? (uintptr_t)ELF_DYN_BASE : 0;
    uintptr_t loaded_pages[ELF_MAX_LOADED_PAGES];
    size_t loaded_count = 0;
    bool loadable = false;

    for (size_t i = 0; i < elf.phnum; i++) {
        struct parsed_segment segment;
        result = parse_segment((const uint8_t*)data, size, &elf, i, &segment);
        if (result < 0) goto rollback;
        if (segment.type != ELF_PT_LOAD || !segment.memsz) continue;
        loadable = true;

        uintptr_t segment_start = (uintptr_t)segment.vaddr + bias;
        if (segment_start < bias) {
            result = -EOVERFLOW;
            goto rollback;
        }
        uintptr_t page_start = segment_start & ~(uintptr_t)(PAGE_SIZE - 1);
        uintptr_t segment_end = segment_start + (uintptr_t)segment.memsz;
        if (segment_end < segment_start) {
            result = -EOVERFLOW;
            goto rollback;
        }
        uintptr_t page_end =
            (segment_end + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
        uint32_t flags = VMM_PRESENT | VMM_USER;
        if (segment.flags & ELF_PF_W) flags |= VMM_WRITABLE;
        if (segment.flags & ELF_PF_X) flags |= VMM_EXECUTABLE;

        for (uintptr_t page = page_start; page < page_end;
             page += PAGE_SIZE) {
            if (vmm_resolve(proc->mm, (void*)page)) continue;
            if (loaded_count >= ELF_MAX_LOADED_PAGES) {
                result = -E2BIG;
                goto rollback;
            }
            result = vmm_map_allocated(proc->mm, (void*)page, flags, NULL);
            if (result < 0) goto rollback;
            loaded_pages[loaded_count++] = page;
        }

        uint64_t copied = 0;
        while (copied < segment.filesz) {
            uintptr_t destination = segment_start + (uintptr_t)copied;
            phys_addr_t physical = vmm_resolve(proc->mm,
                                               (void*)destination);
            if (!physical) {
                result = -EFAULT;
                goto rollback;
            }
            size_t page_remaining =
                PAGE_SIZE - (destination & (PAGE_SIZE - 1));
            size_t chunk = (size_t)(segment.filesz - copied);
            if (chunk > page_remaining) chunk = page_remaining;
            memcpy((void*)(uintptr_t)physical,
                   (const uint8_t*)data + segment.offset + copied, chunk);
            copied += chunk;
        }
    }
    if (!loadable) {
        result = -ENOEXEC;
        goto rollback;
    }
    if ((uintptr_t)elf.entry + bias < bias) {
        result = -EOVERFLOW;
        goto rollback;
    }
    proc->entry_point = (void*)((uintptr_t)elf.entry + bias);
    return 0;

rollback:
    while (loaded_count) {
        vmm_unmap(proc->mm, (void*)loaded_pages[--loaded_count]);
    }
    return result;
}

void* elf_get_entry(const void* data) {
    if (!data) return NULL;
    const uint8_t* ident = (const uint8_t*)data;
    if (memcmp(ident, ELF_MAGIC, 4) != 0) return NULL;
    if (ident[4] == ELF_CLASS_32) {
        return (void*)(uintptr_t)((const struct elf32_header*)data)->entry;
    }
    if (ident[4] == ELF_CLASS_64) {
        return (void*)(uintptr_t)((const struct elf64_header*)data)->entry;
    }
    return NULL;
}
