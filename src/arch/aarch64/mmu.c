#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define A64_VALID (1ULL << 0)
#define A64_TABLE (1ULL << 1)
#define A64_AF (1ULL << 10)
#define A64_SH_INNER (3ULL << 8)
#define A64_ATTR_NORMAL (1ULL << 2)
#define A64_AP_USER (1ULL << 6)
#define A64_AP_RO (1ULL << 7)
#define A64_PXN (1ULL << 53)
#define A64_UXN (1ULL << 54)
#define A64_ADDR_MASK 0x0000fffffffff000ULL

static uint64_t* g_kernel_root;

static uint64_t* allocate_table(void) {
    void* page = pmm_alloc_page();
    if (!page) return NULL;
    memset(page, 0, PAGE_SIZE);
    return (uint64_t*)page;
}

static bool is_table(uint64_t entry, unsigned level) {
    return level < 3 && (entry & (A64_VALID | A64_TABLE)) ==
                                (A64_VALID | A64_TABLE);
}

static uint64_t leaf_flags(uint32_t flags, bool page) {
    uint64_t result = A64_VALID | A64_AF | A64_SH_INNER | A64_ATTR_NORMAL;
    if (page) result |= A64_TABLE;
    if (flags & VMM_USER) result |= A64_AP_USER | A64_PXN;
    else result |= A64_UXN;
    if (!(flags & VMM_WRITABLE)) result |= A64_AP_RO;
    if (!(flags & VMM_EXECUTABLE)) result |= A64_PXN | A64_UXN;
    return result;
}

static uint64_t* clone_level(const uint64_t* source, unsigned level) {
    uint64_t* result = allocate_table();
    if (!result) return NULL;
    for (size_t i = 0; i < 512; i++) {
        if (is_table(source[i], level)) {
            uint64_t* child = clone_level(
                (const uint64_t*)(uintptr_t)(source[i] & A64_ADDR_MASK),
                level + 1);
            if (!child) return NULL;
            result[i] = (uint64_t)(uintptr_t)child | A64_VALID | A64_TABLE;
        } else {
            result[i] = source[i];
        }
    }
    return result;
}

static void destroy_level(uint64_t* table, unsigned level) {
    for (size_t i = 0; i < 512 && level < 3; i++) {
        if (is_table(table[i], level)) {
            destroy_level((uint64_t*)(uintptr_t)(table[i] & A64_ADDR_MASK),
                          level + 1);
        }
    }
    pmm_free_frame((phys_addr_t)(uintptr_t)table);
}

static int split_block(uint64_t* table, size_t index, unsigned level) {
    uint64_t old = table[index];
    uint64_t* child = allocate_table();
    if (!child) return -ENOMEM;
    uint64_t block_size = level == 1 ? (1ULL << 30) : (1ULL << 21);
    uint64_t next_size = block_size >> 9;
    uint64_t base = (old & A64_ADDR_MASK) & ~(block_size - 1);
    uint64_t attrs = old & ~A64_ADDR_MASK;
    for (size_t i = 0; i < 512; i++) {
        child[i] = base + i * next_size + attrs;
        if (level == 2) child[i] |= A64_TABLE;
    }
    table[index] = (uint64_t)(uintptr_t)child | A64_VALID | A64_TABLE;
    return 0;
}

int arch_paging_init(void) {
    g_kernel_root = allocate_table();
    uint64_t* level1 = allocate_table();
    if (!g_kernel_root || !level1) return -ENOMEM;
    g_kernel_root[0] = (uint64_t)(uintptr_t)level1 | A64_VALID | A64_TABLE;
    level1[0] = A64_VALID | A64_AF | A64_SH_INNER | A64_ATTR_NORMAL;

    uint64_t mair = (0x04ULL << 0) | (0xffULL << 8);
    uint64_t tcr = 16ULL | (1ULL << 8) | (1ULL << 10) |
                   (3ULL << 12) | (1ULL << 23);
    uint64_t root = (uint64_t)(uintptr_t)g_kernel_root;
    __asm__ volatile(
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "dsb ish\n"
        "isb\n" :: "r"(mair), "r"(tcr), "r"(root) : "memory");
    uint64_t control;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(control));
    control |= 1ULL;
    __asm__ volatile("msr sctlr_el1, %0\nisb\n" :: "r"(control) : "memory");
    return 0;
}

void* arch_paging_kernel_root(void) { return g_kernel_root; }
void* arch_paging_create_root(void) { return clone_level(g_kernel_root, 0); }
void arch_paging_destroy_root(void* root) {
    if (root && root != g_kernel_root) destroy_level((uint64_t*)root, 0);
}
void arch_paging_switch(void* root) {
    uint64_t address = (uint64_t)(uintptr_t)root;
    __asm__ volatile(
        "msr ttbr0_el1, %0\n"
        "dsb ish\n"
        "tlbi vmalle1is\n"
        "dsb ish\n"
        "isb\n" :: "r"(address) : "memory");
}

int arch_paging_map(void* pointer, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    uint64_t* table = (uint64_t*)pointer;
    const unsigned shifts[4] = {39, 30, 21, 12};
    for (unsigned level = 0; level < 3; level++) {
        size_t index = (virtual_address >> shifts[level]) & 0x1ff;
        uint64_t entry = table[index];
        if ((entry & A64_VALID) && !is_table(entry, level)) {
            if (level == 0) return -ENOTSUP;
            int result = split_block(table, index, level);
            if (result < 0) return result;
        } else if (!(entry & A64_VALID)) {
            uint64_t* child = allocate_table();
            if (!child) return -ENOMEM;
            table[index] = (uint64_t)(uintptr_t)child |
                           A64_VALID | A64_TABLE;
        }
        table = (uint64_t*)(uintptr_t)(table[index] & A64_ADDR_MASK);
    }
    table[(virtual_address >> 12) & 0x1ff] =
        ((uint64_t)physical_address & A64_ADDR_MASK) | leaf_flags(flags, true);
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi vaae1is, %0\n"
        "dsb ish\n"
        "isb\n" :: "r"(virtual_address >> 12) : "memory");
    return 0;
}

int arch_paging_unmap(void* pointer, uintptr_t virtual_address) {
    uint64_t* table = (uint64_t*)pointer;
    const unsigned shifts[3] = {39, 30, 21};
    for (unsigned level = 0; level < 3; level++) {
        uint64_t entry = table[(virtual_address >> shifts[level]) & 0x1ff];
        if (!is_table(entry, level)) return -ENOENT;
        table = (uint64_t*)(uintptr_t)(entry & A64_ADDR_MASK);
    }
    table[(virtual_address >> 12) & 0x1ff] = 0;
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi vaae1is, %0\n"
        "dsb ish\n"
        "isb\n" :: "r"(virtual_address >> 12) : "memory");
    return 0;
}

void mmu_init(void) { (void)arch_paging_init(); }
void mmu_map(uint64_t virt, uint64_t phys) {
    (void)arch_paging_map(g_kernel_root, virt, phys,
                          VMM_PRESENT | VMM_WRITABLE);
}
void mmu_unmap(uint64_t virt) {
    (void)arch_paging_unmap(g_kernel_root, virt);
}
