#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define X86_PTE_PRESENT 0x001u
#define X86_PTE_WRITE   0x002u
#define X86_PTE_USER    0x004u
#define X86_PTE_PCD     0x010u
#define X86_ADDR_MASK   0xfffff000u

static uint32_t* g_kernel_root;

static uint32_t* allocate_table(void) {
    phys_addr_t frame = pmm_alloc_frame();
    if (!frame) return NULL;
    uint32_t* table = (uint32_t*)(uintptr_t)frame;
    memset(table, 0, PAGE_SIZE);
    return table;
}

static uint32_t native_flags(uint32_t flags) {
    uint32_t result = X86_PTE_PRESENT;
    if (flags & VMM_WRITABLE) result |= X86_PTE_WRITE;
    if (flags & VMM_USER) result |= X86_PTE_USER;
    if (flags & VMM_NOCACHE) result |= X86_PTE_PCD;
    return result;
}

int arch_paging_init(void) {
    g_kernel_root = allocate_table();
    if (!g_kernel_root) return -ENOMEM;
    for (uintptr_t address = 0; address < 16 * 1024 * 1024;
         address += PAGE_SIZE) {
        size_t directory_index = address >> 22;
        if (!(g_kernel_root[directory_index] & X86_PTE_PRESENT)) {
            uint32_t* table = allocate_table();
            if (!table) return -ENOMEM;
            g_kernel_root[directory_index] =
                (uint32_t)(uintptr_t)table | X86_PTE_PRESENT | X86_PTE_WRITE;
        }
        uint32_t* table = (uint32_t*)(uintptr_t)
            (g_kernel_root[directory_index] & X86_ADDR_MASK);
        table[(address >> 12) & 0x3ff] =
            (uint32_t)address | X86_PTE_PRESENT | X86_PTE_WRITE;
    }
    uintptr_t root = (uintptr_t)g_kernel_root;
    __asm__ volatile("mov %0, %%cr3" :: "r"(root) : "memory");
    uintptr_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
    return 0;
}

void* arch_paging_kernel_root(void) {
    return g_kernel_root;
}

void* arch_paging_create_root(void) {
    uint32_t* root = allocate_table();
    if (!root) return NULL;
    for (size_t i = 0; i < 1024; i++) {
        if (!(g_kernel_root[i] & X86_PTE_PRESENT)) continue;
        uint32_t* table = allocate_table();
        if (!table) return NULL;
        memcpy(table, (void*)(uintptr_t)(g_kernel_root[i] & X86_ADDR_MASK),
               PAGE_SIZE);
        root[i] = (uint32_t)(uintptr_t)table |
                  (g_kernel_root[i] & ~X86_ADDR_MASK);
    }
    return root;
}

void arch_paging_destroy_root(void* pointer) {
    uint32_t* root = (uint32_t*)pointer;
    if (!root || root == g_kernel_root) return;
    for (size_t i = 0; i < 1024; i++) {
        if (root[i] & X86_PTE_PRESENT) {
            pmm_free_frame(root[i] & X86_ADDR_MASK);
        }
    }
    pmm_free_frame((phys_addr_t)(uintptr_t)root);
}

void arch_paging_switch(void* root) {
    uintptr_t address = (uintptr_t)root;
    __asm__ volatile("mov %0, %%cr3" :: "r"(address) : "memory");
}

int arch_paging_map(void* pointer, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    uint32_t* root = (uint32_t*)pointer;
    size_t directory_index = virtual_address >> 22;
    uint32_t table_flags = X86_PTE_PRESENT | X86_PTE_WRITE |
                           ((flags & VMM_USER) ? X86_PTE_USER : 0);
    if (!(root[directory_index] & X86_PTE_PRESENT)) {
        uint32_t* table = allocate_table();
        if (!table) return -ENOMEM;
        root[directory_index] = (uint32_t)(uintptr_t)table | table_flags;
    } else {
        root[directory_index] |= table_flags;
    }
    uint32_t* table =
        (uint32_t*)(uintptr_t)(root[directory_index] & X86_ADDR_MASK);
    table[(virtual_address >> 12) & 0x3ff] =
        ((uint32_t)physical_address & X86_ADDR_MASK) | native_flags(flags);
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_address) : "memory");
    return 0;
}

int arch_paging_unmap(void* pointer, uintptr_t virtual_address) {
    uint32_t* root = (uint32_t*)pointer;
    uint32_t entry = root[virtual_address >> 22];
    if (!(entry & X86_PTE_PRESENT)) return -ENOENT;
    uint32_t* table = (uint32_t*)(uintptr_t)(entry & X86_ADDR_MASK);
    table[(virtual_address >> 12) & 0x3ff] = 0;
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_address) : "memory");
    return 0;
}

void paging_init(void) {
    (void)arch_paging_init();
}

void page_map(uint32_t virt, uint32_t phys) {
    (void)arch_paging_map(g_kernel_root, virt, phys,
                          VMM_PRESENT | VMM_WRITABLE);
}

void page_unmap(uint32_t virt) {
    (void)arch_paging_unmap(g_kernel_root, virt);
}
