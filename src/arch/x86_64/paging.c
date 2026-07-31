#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define X86_PTE_PRESENT (1ULL << 0)
#define X86_PTE_WRITE   (1ULL << 1)
#define X86_PTE_USER    (1ULL << 2)
#define X86_PTE_PCD     (1ULL << 4)
#define X86_PTE_HUGE    (1ULL << 7)
#define X86_PTE_NX      (1ULL << 63)
#define X86_ADDR_MASK   0x000ffffffffff000ULL

static uint64_t* g_kernel_root;

static uint64_t* allocate_table(void) {
    phys_addr_t frame = pmm_alloc_frame();
    if (!frame) {
        return NULL;
    }
    uint64_t* table = (uint64_t*)(uintptr_t)frame;
    memset(table, 0, PAGE_SIZE);
    return table;
}

static uint64_t native_flags(uint32_t flags) {
    uint64_t result = X86_PTE_PRESENT;
    if (flags & VMM_WRITABLE) result |= X86_PTE_WRITE;
    if (flags & VMM_USER) result |= X86_PTE_USER;
    if (flags & VMM_NOCACHE) result |= X86_PTE_PCD;
    if (!(flags & VMM_EXECUTABLE)) result |= X86_PTE_NX;
    return result;
}

static uint64_t* clone_level(const uint64_t* source, unsigned level) {
    uint64_t* destination = allocate_table();
    if (!destination) {
        return NULL;
    }
    for (size_t i = 0; i < 512; i++) {
        uint64_t entry = source[i];
        if (!(entry & X86_PTE_PRESENT) || level == 1 ||
            (entry & X86_PTE_HUGE)) {
            destination[i] = entry;
            continue;
        }
        uint64_t* child = clone_level(
            (const uint64_t*)(uintptr_t)(entry & X86_ADDR_MASK), level - 1);
        if (!child) {
            pmm_free_frame((phys_addr_t)(uintptr_t)destination);
            return NULL;
        }
        destination[i] = ((uint64_t)(uintptr_t)child & X86_ADDR_MASK) |
                         (entry & ~X86_ADDR_MASK);
    }
    return destination;
}

static void destroy_level(uint64_t* table, unsigned level) {
    if (!table) {
        return;
    }
    if (level > 1) {
        for (size_t i = 0; i < 512; i++) {
            uint64_t entry = table[i];
            if ((entry & X86_PTE_PRESENT) && !(entry & X86_PTE_HUGE)) {
                destroy_level((uint64_t*)(uintptr_t)(entry & X86_ADDR_MASK),
                              level - 1);
            }
        }
    }
    pmm_free_frame((phys_addr_t)(uintptr_t)table);
}

static int split_2m_page(uint64_t* directory, size_t index) {
    uint64_t old = directory[index];
    uint64_t* table = allocate_table();
    if (!table) {
        return -ENOMEM;
    }
    uint64_t base = old & 0x000fffffffe00000ULL;
    uint64_t flags = old & ~0x000fffffffe00000ULL;
    flags &= ~X86_PTE_HUGE;
    for (size_t i = 0; i < 512; i++) {
        table[i] = base + i * PAGE_SIZE + flags;
    }
    directory[index] = ((uint64_t)(uintptr_t)table & X86_ADDR_MASK) |
                       X86_PTE_PRESENT | X86_PTE_WRITE |
                       (old & X86_PTE_USER);
    return 0;
}

int arch_paging_init(void) {
    uintptr_t root;
    __asm__ volatile("mov %%cr3, %0" : "=r"(root));
    g_kernel_root = (uint64_t*)(root & X86_ADDR_MASK);

    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0xc0000080));
    low |= (1u << 11);
    __asm__ volatile("wrmsr" :: "a"(low), "d"(high), "c"(0xc0000080));
    return g_kernel_root ? 0 : -EINVAL;
}

void* arch_paging_kernel_root(void) {
    return g_kernel_root;
}

void* arch_paging_create_root(void) {
    return clone_level(g_kernel_root, 4);
}

void arch_paging_destroy_root(void* root) {
    if (root && root != g_kernel_root) {
        destroy_level((uint64_t*)root, 4);
    }
}

void arch_paging_switch(void* root) {
    uintptr_t address = (uintptr_t)root;
    __asm__ volatile("mov %0, %%cr3" :: "r"(address) : "memory");
}

int arch_paging_map(void* root, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    uint64_t* pml4 = (uint64_t*)root;
    size_t i4 = (virtual_address >> 39) & 0x1ff;
    size_t i3 = (virtual_address >> 30) & 0x1ff;
    size_t i2 = (virtual_address >> 21) & 0x1ff;
    size_t i1 = (virtual_address >> 12) & 0x1ff;
    uint64_t table_flags = X86_PTE_PRESENT | X86_PTE_WRITE |
                           ((flags & VMM_USER) ? X86_PTE_USER : 0);

    if (!(pml4[i4] & X86_PTE_PRESENT)) {
        uint64_t* table = allocate_table();
        if (!table) return -ENOMEM;
        pml4[i4] = (uint64_t)(uintptr_t)table | table_flags;
    } else {
        pml4[i4] |= table_flags;
    }
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4[i4] & X86_ADDR_MASK);
    if (!(pdpt[i3] & X86_PTE_PRESENT)) {
        uint64_t* table = allocate_table();
        if (!table) return -ENOMEM;
        pdpt[i3] = (uint64_t)(uintptr_t)table | table_flags;
    } else if (pdpt[i3] & X86_PTE_HUGE) {
        return -ENOTSUP;
    } else {
        pdpt[i3] |= table_flags;
    }
    uint64_t* pd = (uint64_t*)(uintptr_t)(pdpt[i3] & X86_ADDR_MASK);
    if (pd[i2] & X86_PTE_HUGE) {
        int result = split_2m_page(pd, i2);
        if (result < 0) return result;
    } else if (!(pd[i2] & X86_PTE_PRESENT)) {
        uint64_t* table = allocate_table();
        if (!table) return -ENOMEM;
        pd[i2] = (uint64_t)(uintptr_t)table | table_flags;
    } else {
        pd[i2] |= table_flags;
    }
    uint64_t* pt = (uint64_t*)(uintptr_t)(pd[i2] & X86_ADDR_MASK);
    pt[i1] = ((uint64_t)physical_address & X86_ADDR_MASK) |
             native_flags(flags);
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_address) : "memory");
    return 0;
}

int arch_paging_unmap(void* root, uintptr_t virtual_address) {
    uint64_t* pml4 = (uint64_t*)root;
    uint64_t e4 = pml4[(virtual_address >> 39) & 0x1ff];
    if (!(e4 & X86_PTE_PRESENT)) return -ENOENT;
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(e4 & X86_ADDR_MASK);
    uint64_t e3 = pdpt[(virtual_address >> 30) & 0x1ff];
    if (!(e3 & X86_PTE_PRESENT) || (e3 & X86_PTE_HUGE)) return -ENOENT;
    uint64_t* pd = (uint64_t*)(uintptr_t)(e3 & X86_ADDR_MASK);
    uint64_t e2 = pd[(virtual_address >> 21) & 0x1ff];
    if (!(e2 & X86_PTE_PRESENT) || (e2 & X86_PTE_HUGE)) return -ENOENT;
    uint64_t* pt = (uint64_t*)(uintptr_t)(e2 & X86_ADDR_MASK);
    pt[(virtual_address >> 12) & 0x1ff] = 0;
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_address) : "memory");
    return 0;
}

/* Compatibility entry points retained for older architecture callers. */
void paging_init(void) {
    (void)arch_paging_init();
}

void page_map(uint64_t virt, uint64_t phys) {
    (void)arch_paging_map(g_kernel_root, virt, phys,
                          VMM_PRESENT | VMM_WRITABLE);
}

void page_unmap(uint64_t virt) {
    (void)arch_paging_unmap(g_kernel_root, virt);
}
