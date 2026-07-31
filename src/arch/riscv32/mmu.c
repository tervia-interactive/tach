#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <hal/mm.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define RV_PTE_V 0x001u
#define RV_PTE_R 0x002u
#define RV_PTE_W 0x004u
#define RV_PTE_X 0x008u
#define RV_PTE_U 0x010u
#define RV_PTE_A 0x040u
#define RV_PTE_D 0x080u

static uint32_t* g_kernel_root;

static uint32_t* allocate_table(void) {
    void* page = pmm_alloc_page();
    if (!page) return NULL;
    memset(page, 0, PAGE_SIZE);
    return (uint32_t*)page;
}

static bool is_leaf(uint32_t entry) {
    return (entry & (RV_PTE_R | RV_PTE_W | RV_PTE_X)) != 0;
}

static uint32_t leaf_flags(uint32_t flags) {
    uint32_t result = RV_PTE_V | RV_PTE_R | RV_PTE_A;
    if (flags & VMM_WRITABLE) result |= RV_PTE_W | RV_PTE_D;
    if (flags & VMM_EXECUTABLE) result |= RV_PTE_X;
    if (flags & VMM_USER) result |= RV_PTE_U;
    return result;
}

static int split_megapage(uint32_t* root, size_t index) {
    uint32_t old = root[index];
    uint32_t* table = allocate_table();
    if (!table) return -ENOMEM;
    uint32_t base = ((old >> 10) << 12) & 0xffc00000u;
    uint32_t attrs = old & 0x3ffu;
    for (size_t i = 0; i < 1024; i++) {
        uint32_t physical = base + (uint32_t)(i * PAGE_SIZE);
        table[i] = (physical >> 12 << 10) | attrs;
    }
    root[index] = ((uint32_t)(uintptr_t)table >> 12 << 10) | RV_PTE_V;
    return 0;
}

int arch_paging_init(void) {
    g_kernel_root = allocate_table();
    if (!g_kernel_root) return -ENOMEM;
    for (uint32_t address = 0x80000000u; address < 0x88000000u;
         address += 0x00400000u) {
        g_kernel_root[address >> 22] =
            (address >> 12 << 10) |
            RV_PTE_V | RV_PTE_R | RV_PTE_W | RV_PTE_X | RV_PTE_A | RV_PTE_D;
    }
    arch_paging_switch(g_kernel_root);
    return 0;
}

void* arch_paging_kernel_root(void) { return g_kernel_root; }
void* arch_paging_create_root(void) {
    uint32_t* root = allocate_table();
    if (root) memcpy(root, g_kernel_root, PAGE_SIZE);
    return root;
}
void arch_paging_destroy_root(void* pointer) {
    uint32_t* root = (uint32_t*)pointer;
    if (!root || root == g_kernel_root) return;
    for (size_t i = 0; i < 1024; i++) {
        if ((root[i] & RV_PTE_V) && !is_leaf(root[i])) {
            pmm_free_frame((phys_addr_t)(root[i] >> 10 << 12));
        }
    }
    pmm_free_frame((phys_addr_t)(uintptr_t)root);
}
void arch_paging_switch(void* root) {
    uint32_t satp = ((uint32_t)(uintptr_t)root >> 12) | (1u << 31);
    __asm__ volatile("csrw satp, %0\nsfence.vma" :: "r"(satp) : "memory");
}
int arch_paging_map(void* pointer, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    uint32_t* root = (uint32_t*)pointer;
    size_t root_index = virtual_address >> 22;
    if ((root[root_index] & RV_PTE_V) && is_leaf(root[root_index])) {
        int result = split_megapage(root, root_index);
        if (result < 0) return result;
    } else if (!(root[root_index] & RV_PTE_V)) {
        uint32_t* table = allocate_table();
        if (!table) return -ENOMEM;
        root[root_index] =
            ((uint32_t)(uintptr_t)table >> 12 << 10) | RV_PTE_V;
    }
    uint32_t* table = (uint32_t*)(uintptr_t)
        (root[root_index] >> 10 << 12);
    table[(virtual_address >> 12) & 0x3ff] =
        ((uint32_t)physical_address >> 12 << 10) | leaf_flags(flags);
    __asm__ volatile("sfence.vma %0, zero" :: "r"(virtual_address) : "memory");
    return 0;
}
int arch_paging_unmap(void* pointer, uintptr_t virtual_address) {
    uint32_t* root = (uint32_t*)pointer;
    uint32_t entry = root[virtual_address >> 22];
    if (!(entry & RV_PTE_V) || is_leaf(entry)) return -ENOENT;
    uint32_t* table = (uint32_t*)(uintptr_t)(entry >> 10 << 12);
    table[(virtual_address >> 12) & 0x3ff] = 0;
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
