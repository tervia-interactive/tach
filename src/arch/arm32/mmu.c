#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define ARM_L1_COARSE 0x001u
#define ARM_L1_SECTION 0x002u
#define ARM_L1_ADDR 0xfffffc00u
#define ARM_L2_SMALL 0x002u
#define ARM_L2_ADDR 0xfffff000u

static uint32_t* g_kernel_root;

static uint32_t* allocate_l1(void) {
    void* pages = pmm_alloc_aligned_pages(4, 4);
    if (!pages) return NULL;
    memset(pages, 0, PAGE_SIZE * 4);
    return (uint32_t*)pages;
}

static uint32_t* allocate_l2(void) {
    void* page = pmm_alloc_page();
    if (!page) return NULL;
    memset(page, 0, PAGE_SIZE);
    return (uint32_t*)page;
}

static uint32_t small_flags(uint32_t flags) {
    uint32_t descriptor = ARM_L2_SMALL;
    if (flags & VMM_USER) {
        descriptor |= (flags & VMM_WRITABLE) ? (3u << 4) : (2u << 4);
    } else {
        descriptor |= 1u << 4;
    }
    if (!(flags & VMM_WRITABLE)) descriptor |= (1u << 9);
    if (!(flags & VMM_EXECUTABLE)) descriptor |= 1u;
    return descriptor;
}

static int split_section(uint32_t* root, size_t index) {
    uint32_t old = root[index];
    uint32_t* table = allocate_l2();
    if (!table) return -ENOMEM;
    uint32_t base = old & 0xfff00000u;
    for (size_t i = 0; i < 256; i++) {
        table[i] = (base + (uint32_t)(i * PAGE_SIZE)) |
                   ARM_L2_SMALL | (1u << 4);
    }
    root[index] = ((uint32_t)(uintptr_t)table & ARM_L1_ADDR) | ARM_L1_COARSE;
    return 0;
}

int arch_paging_init(void) {
    g_kernel_root = allocate_l1();
    if (!g_kernel_root) return -ENOMEM;
    /* Keep QEMU virt RAM plus GICv2 (0x08000000) and PL011 (0x09000000)
     * identity-mapped in the privileged half of every address space. */
    for (uint32_t address = 0; address < 0x10000000u;
        address += 0x00100000u) {
        g_kernel_root[address >> 20] =
            address | ARM_L1_SECTION | (1u << 10);
    }
    for (uint32_t address = 0x40000000u; address < 0x48000000u;
         address += 0x00100000u) {
        g_kernel_root[address >> 20] =
            address | ARM_L1_SECTION | (1u << 10) | (1u << 3) | (1u << 2);
    }
    uintptr_t root = (uintptr_t)g_kernel_root;
    uint32_t domain = 1;
    uint32_t zero = 0;
    __asm__ volatile(
        "mcr p15, 0, %0, c2, c0, 0\n"
        "mcr p15, 0, %1, c2, c0, 2\n"
        "mcr p15, 0, %2, c3, c0, 0\n"
        "dsb\n"
        "isb\n"
        :: "r"(root), "r"(zero), "r"(domain) : "memory");
    uint32_t control;
    __asm__ volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(control));
    control |= 1u;
    __asm__ volatile(
        "mcr p15, 0, %0, c1, c0, 0\n"
        "isb\n" :: "r"(control) : "memory");
    return 0;
}

void* arch_paging_kernel_root(void) { return g_kernel_root; }

void* arch_paging_create_root(void) {
    uint32_t* root = allocate_l1();
    if (root) memcpy(root, g_kernel_root, PAGE_SIZE * 4);
    return root;
}

void arch_paging_destroy_root(void* pointer) {
    uint32_t* root = (uint32_t*)pointer;
    if (!root || root == g_kernel_root) return;
    for (size_t i = 0; i < 4096; i++) {
        if ((root[i] & 3u) == ARM_L1_COARSE) {
            pmm_free_frame(root[i] & ARM_L1_ADDR);
        }
    }
    pmm_free_pages(root, 4);
}

void arch_paging_switch(void* root) {
    uintptr_t address = (uintptr_t)root;
    __asm__ volatile(
        "mcr p15, 0, %0, c2, c0, 0\n"
        "dsb\n"
        "mcr p15, 0, %0, c8, c7, 0\n"
        "isb\n" :: "r"(address) : "memory");
}

int arch_paging_map(void* pointer, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    uint32_t* root = (uint32_t*)pointer;
    size_t l1_index = virtual_address >> 20;
    if ((root[l1_index] & 3u) == ARM_L1_SECTION) {
        int result = split_section(root, l1_index);
        if (result < 0) return result;
    } else if ((root[l1_index] & 3u) != ARM_L1_COARSE) {
        uint32_t* table = allocate_l2();
        if (!table) return -ENOMEM;
        root[l1_index] =
            ((uint32_t)(uintptr_t)table & ARM_L1_ADDR) | ARM_L1_COARSE;
    }
    uint32_t* table =
        (uint32_t*)(uintptr_t)(root[l1_index] & ARM_L1_ADDR);
    table[(virtual_address >> 12) & 0xff] =
        ((uint32_t)physical_address & ARM_L2_ADDR) | small_flags(flags);
    __asm__ volatile(
        "mcr p15, 0, %0, c8, c7, 1\n"
        "dsb\n"
        "isb\n" :: "r"(virtual_address) : "memory");
    return 0;
}

int arch_paging_unmap(void* pointer, uintptr_t virtual_address) {
    uint32_t* root = (uint32_t*)pointer;
    uint32_t entry = root[virtual_address >> 20];
    if ((entry & 3u) != ARM_L1_COARSE) return -ENOENT;
    uint32_t* table = (uint32_t*)(uintptr_t)(entry & ARM_L1_ADDR);
    table[(virtual_address >> 12) & 0xff] = 0;
    __asm__ volatile(
        "mcr p15, 0, %0, c8, c7, 1\n"
        "dsb\n"
        "isb\n" :: "r"(virtual_address) : "memory");
    return 0;
}

void mmu_init(void) { (void)arch_paging_init(); }
void mmu_map(uint32_t virt, uint32_t phys) {
    (void)arch_paging_map(g_kernel_root, virt, phys,
                          VMM_PRESENT | VMM_WRITABLE);
}
void mmu_unmap(uint32_t virt) {
    (void)arch_paging_unmap(g_kernel_root, virt);
}
