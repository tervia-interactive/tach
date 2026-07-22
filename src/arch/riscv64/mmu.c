/* RISC-V 64-bit MMU Implementation */

#include <hal/mm.h>
#include <kernel/types.h>
#include <kernel/panic.h>

#define RV64_PGSHIFT 12
#define RV64_PGSIZE (1UL << RV64_PGSHIFT)
#define RV64_PTE_V 0x01
#define RV64_PTE_R 0x02
#define RV64_PTE_W 0x04
#define RV64_PTE_X 0x08
#define RV64_PTE_U 0x10

static uint64_t *root_page_table = NULL;

int hal_mmu_init(void) {
    root_page_table = (uint64_t *)0x80000000;
    
    for (size_t i = 0; i < 512; i++) {
        root_page_table[i] = 0;
    }
    
    return 0;
}

int hal_mmu_map(uintptr_t vaddr, uintptr_t paddr, size_t size, uint32_t flags) {
    if (!root_page_table) return -1;
    
    size_t num_pages = (size + RV64_PGSIZE - 1) / RV64_PGSIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t vpn = (vaddr + i * RV64_PGSIZE) >> RV64_PGSHIFT;
        uint64_t ppn = (paddr + i * RV64_PGSIZE) >> RV64_PGSHIFT;
        
        root_page_table[vpn] = (ppn << 10) | flags | RV64_PTE_V;
    }
    
    return 0;
}

int hal_mmu_unmap(uintptr_t vaddr, size_t size) {
    if (!root_page_table) return -1;
    
    size_t num_pages = (size + RV64_PGSIZE - 1) / RV64_PGSIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t vpn = (vaddr + i * RV64_PGSIZE) >> RV64_PGSHIFT;
        root_page_table[vpn] = 0;
    }
    
    return 0;
}

void hal_mmu_enable(void) {
    uint64_t satp = ((uint64_t)root_page_table >> RV64_PGSHIFT) | (9UL << 60);
    __asm__ volatile("csrw satp, %0" :: "r"(satp));
    __asm__ volatile("sfence.vma");
}
