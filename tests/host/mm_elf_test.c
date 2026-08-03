#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/mm.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/elf.h>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)
#define MOCK_PAGES 8

struct mock_mapping {
    uintptr_t virt;
    uint8_t page[PAGE_SIZE];
    bool used;
};

static struct mock_mapping g_mappings[MOCK_PAGES];

int vmm_map_allocated(struct vmm_context* context, void* virt, uint32_t flags,
                      phys_addr_t* phys_out) {
    (void)context;
    (void)flags;
    for (size_t i = 0; i < MOCK_PAGES; i++) {
        if (g_mappings[i].used) continue;
        g_mappings[i].used = true;
        g_mappings[i].virt = (uintptr_t)virt;
        memset(g_mappings[i].page, 0, PAGE_SIZE);
        if (phys_out) *phys_out = (phys_addr_t)(uintptr_t)g_mappings[i].page;
        return 0;
    }
    return -ENOMEM;
}

phys_addr_t vmm_resolve(struct vmm_context* context, const void* address) {
    (void)context;
    uintptr_t pointer = (uintptr_t)address;
    uintptr_t page = pointer & ~(uintptr_t)(PAGE_SIZE - 1);
    for (size_t i = 0; i < MOCK_PAGES; i++) {
        if (g_mappings[i].used && g_mappings[i].virt == page) {
            return (phys_addr_t)(uintptr_t)
                (g_mappings[i].page + (pointer & (PAGE_SIZE - 1)));
        }
    }
    return 0;
}

int vmm_unmap(struct vmm_context* context, void* virt) {
    (void)context;
    for (size_t i = 0; i < MOCK_PAGES; i++) {
        if (g_mappings[i].used &&
            g_mappings[i].virt == (uintptr_t)virt) {
            g_mappings[i].used = false;
            return 0;
        }
    }
    return -ENOENT;
}

static uint16_t native_machine(void) {
#if defined(__x86_64__)
    return ELF_MACHINE_X86_64;
#else
    return ELF_MACHINE_X86;
#endif
}

static int test_pmm(void) {
    mem_region_t map[] = {
        { .base = 0, .size = 0x100000, .type = MEM_REGION_RESERVED },
        { .base = 0x100000, .size = 64 * PAGE_SIZE,
          .type = MEM_REGION_USABLE },
        { .base = 0x110000, .size = 2 * PAGE_SIZE,
          .type = MEM_REGION_RESERVED },
    };
    pmm_init(map, sizeof(map) / sizeof(map[0]));
    CHECK(pmm_get_total_pages() == 62);
    CHECK(pmm_get_free_pages() == 62);
    phys_addr_t first = pmm_alloc_frame();
    phys_addr_t second = pmm_alloc_frame();
    CHECK(first == 0x100000);
    CHECK(second == 0x101000);
    CHECK(pmm_frame_refcount(first) == 1);
    CHECK(pmm_retain_frame(first));
    CHECK(pmm_frame_refcount(first) == 2);
    pmm_free_frame(first);
    CHECK(pmm_frame_refcount(first) == 1);
    pmm_free_frame(first);
    CHECK(pmm_alloc_frame() == first);
    void* contiguous = pmm_alloc_aligned_pages(4, 4);
    CHECK(contiguous != NULL);
    CHECK(((uintptr_t)contiguous & (4 * PAGE_SIZE - 1)) == 0);
    return 0;
}

static int test_elf(void) {
    uint8_t image[512];
    memset(image, 0, sizeof(image));
    struct elf64_header* header = (struct elf64_header*)image;
    memcpy(header->ident, ELF_MAGIC, 4);
    header->ident[4] = ELF_CLASS_64;
    header->ident[5] = ELF_DATA_LSB;
    header->ident[6] = ELF_VERSION_CURRENT;
    header->type = ELF_TYPE_EXEC;
    header->machine = native_machine();
    header->version = ELF_VERSION_CURRENT;
    header->entry = 0x400000;
    header->phoff = sizeof(*header);
    header->ehsize = sizeof(*header);
    header->phentsize = sizeof(struct elf64_program_header);
    header->phnum = 1;

    struct elf64_program_header* program =
        (struct elf64_program_header*)(image + header->phoff);
    program->type = ELF_PT_LOAD;
    program->flags = ELF_PF_R | ELF_PF_X;
    program->offset = 256;
    program->vaddr = 0x400000;
    program->filesz = 3;
    program->memsz = PAGE_SIZE + 8;
    program->align = PAGE_SIZE;
    memcpy(image + 256, "ELF", 3);

    CHECK(elf_validate(image, sizeof(image)) == 0);
    struct vmm_context context;
    memset(&context, 0, sizeof(context));
    struct process process;
    memset(&process, 0, sizeof(process));
    process.mm = &context;
    CHECK(elf_load(image, sizeof(image), &process) == 0);
    CHECK(process.entry_point == (void*)(uintptr_t)0x400000);
    CHECK(memcmp((void*)(uintptr_t)vmm_resolve(&context,
                                               (void*)0x400000),
                 "ELF", 3) == 0);
    CHECK(*(uint8_t*)(uintptr_t)vmm_resolve(&context,
                                            (void*)0x400003) == 0);
    CHECK(vmm_resolve(&context, (void*)0x401000) != 0);

    image[0] = 0;
    CHECK(elf_validate(image, sizeof(image)) == -ENOEXEC);
    return 0;
}

int main(void) {
    int result = test_pmm();
    if (result) return result;
    return test_elf();
}
