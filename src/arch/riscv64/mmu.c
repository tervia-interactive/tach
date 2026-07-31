#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <hal/mm.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define RV_PTE_V 0x001ULL
#define RV_PTE_R 0x002ULL
#define RV_PTE_W 0x004ULL
#define RV_PTE_X 0x008ULL
#define RV_PTE_U 0x010ULL
#define RV_PTE_A 0x040ULL
#define RV_PTE_D 0x080ULL
#define RV_PTE_ADDR(entry) (((entry) >> 10) << 12)

static uint64_t* g_kernel_root;

static uint64_t* allocate_table(void) {
    void* page = pmm_alloc_page();
    if (!page) return NULL;
    memset(page, 0, PAGE_SIZE);
    return (uint64_t*)page;
}

static bool is_leaf(uint64_t entry) {
    return (entry & (RV_PTE_R | RV_PTE_W | RV_PTE_X)) != 0;
}

static uint64_t leaf_flags(uint32_t flags) {
    uint64_t result = RV_PTE_V | RV_PTE_R | RV_PTE_A;
    if (flags & VMM_WRITABLE) result |= RV_PTE_W | RV_PTE_D;
    if (flags & VMM_EXECUTABLE) result |= RV_PTE_X;
    if (flags & VMM_USER) result |= RV_PTE_U;
    return result;
}

static int split_leaf(uint64_t* table, size_t index, unsigned level) {
    uint64_t old = table[index];
    uint64_t* child = allocate_table();
    if (!child) return -ENOMEM;
    uint64_t block_size = level == 2 ? (1ULL << 30) : (1ULL << 21);
    uint64_t next_size = block_size >> 9;
    uint64_t base = RV_PTE_ADDR(old) & ~(block_size - 1);
    uint64_t attrs = old & 0x3ff;
    for (size_t i = 0; i < 512; i++) {
        child[i] = ((base + i * next_size) >> 12 << 10) | attrs;
    }
    table[index] = ((uint64_t)(uintptr_t)child >> 12 << 10) | RV_PTE_V;
    return 0;
}

static void destroy_tables(uint64_t* table, unsigned level) {
    if (level) {
        for (size_t i = 0; i < 512; i++) {
            if ((table[i] & RV_PTE_V) && !is_leaf(table[i])) {
                destroy_tables((uint64_t*)(uintptr_t)RV_PTE_ADDR(table[i]),
                               level - 1);
            }
        }
    }
    pmm_free_frame((phys_addr_t)(uintptr_t)table);
}

int arch_paging_init(void) {
    g_kernel_root = allocate_table();
    if (!g_kernel_root) return -ENOMEM;
    size_t index = (0x80000000ULL >> 30) & 0x1ff;
    g_kernel_root[index] =
        (0x80000000ULL >> 12 << 10) |
        RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_X | RV_PTE_A | RV_PTE_D;
    arch_paging_switch(g_kernel_root);
    return 0;
}
void* arch_paging_kernel_root(void) { return g_kernel_root; }
void* arch_paging_create_root(void) {
    uint64_t* root = allocate_table();
    if (root) memcpy(root, g_kernel_root, PAGE_SIZE);
    return root;
}
void arch_paging_destroy_root(void* pointer) {
    uint64_t* root = (uint64_t*)pointer;
    if (!root || root == g_kernel_root) return;
    for (size_t i = 0; i < 512; i++) {
        if ((root[i] & RV_PTE_V) && !is_leaf(root[i])) {
            destroy_tables((uint64_t*)(uintptr_t)RV_PTE_ADDR(root[i]), 1);
        }
    }
    pmm_free_frame((phys_addr_t)(uintptr_t)root);
}
void arch_paging_switch(void* root) {
    uint64_t satp = ((uint64_t)(uintptr_t)root >> 12) | (8ULL << 60);
    __asm__ volatile("csrw satp, %0\nsfence.vma" :: "r"(satp) : "memory");
}
int arch_paging_map(void* pointer, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    uint64_t* table = (uint64_t*)pointer;
    const unsigned shifts[3] = {30, 21, 12};
    for (unsigned depth = 0; depth < 2; depth++) {
        unsigned level = 2 - depth;
        size_t index = (virtual_address >> shifts[depth]) & 0x1ff;
        if ((table[index] & RV_PTE_V) && is_leaf(table[index])) {
            int result = split_leaf(table, index, level);
            if (result < 0) return result;
        } else if (!(table[index] & RV_PTE_V)) {
            uint64_t* child = allocate_table();
            if (!child) return -ENOMEM;
            table[index] =
                ((uint64_t)(uintptr_t)child >> 12 << 10) | RV_PTE_V;
        }
        table = (uint64_t*)(uintptr_t)RV_PTE_ADDR(table[index]);
    }
    table[(virtual_address >> 12) & 0x1ff] =
        ((uint64_t)physical_address >> 12 << 10) | leaf_flags(flags);
    __asm__ volatile("sfence.vma %0, zero" :: "r"(virtual_address) : "memory");
    return 0;
}
int arch_paging_unmap(void* pointer, uintptr_t virtual_address) {
    uint64_t* table = (uint64_t*)pointer;
    const unsigned shifts[2] = {30, 21};
    for (unsigned depth = 0; depth < 2; depth++) {
        uint64_t entry = table[(virtual_address >> shifts[depth]) & 0x1ff];
        if (!(entry & RV_PTE_V) || is_leaf(entry)) return -ENOENT;
        table = (uint64_t*)(uintptr_t)RV_PTE_ADDR(entry);
    }
    table[(virtual_address >> 12) & 0x1ff] = 0;
    __asm__ volatile("sfence.vma %0, zero" :: "r"(virtual_address) : "memory");
    return 0;
}

void hal_mmu_init(void) { (void)arch_paging_init(); }
int hal_mmu_map(uintptr_t virt, uintptr_t phys, size_t size, uint32_t flags) {
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        int result = arch_paging_map(g_kernel_root, virt + offset,
                                     phys + offset, flags);
        if (result < 0) return result;
    }
    return 0;
}
int hal_mmu_unmap(uintptr_t virt, size_t size) {
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        int result = arch_paging_unmap(g_kernel_root, virt + offset);
        if (result < 0) return result;
    }
    return 0;
}
void hal_mmu_enable(void) { arch_paging_switch(g_kernel_root); }
