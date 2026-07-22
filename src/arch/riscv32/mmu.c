/* RISC-V 32-bit MMU Implementation */

#include <hal/mm.h>
#include <kernel/types.h>
#include <kernel/panic.h>

#define RV32_PGSHIFT 12
#define RV32_PGSIZE (1 << RV32_PGSHIFT)
#define RV32_PTE_V 0x01
#define RV32_PTE_R 0x02
#define RV32_PTE_W 0x04
#define RV32_PTE_X 0x08
#define RV32_PTE_U 0x10

static uint32_t *root_page_table = NULL;

void hal_mmu_init(void) {
    root_page_table = (uint32_t *)0x80000000;
    
    for (size_t i = 0; i < 1024; i++) {
        root_page_table[i] = 0;
    }
}

int hal_mmu_map(uintptr_t vaddr, uintptr_t paddr, size_t size, uint32_t flags) {
    if (!root_page_table) return -1;
    
    size_t num_pages = (size + RV32_PGSIZE - 1) / RV32_PGSIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t vpn = (vaddr + i * RV32_PGSIZE) >> RV32_PGSHIFT;
        uint32_t ppn = (paddr + i * RV32_PGSIZE) >> RV32_PGSHIFT;
        
        root_page_table[vpn] = (ppn << 10) | flags | RV32_PTE_V;
    }
    
    return 0;
}

int hal_mmu_unmap(uintptr_t vaddr, size_t size) {
    if (!root_page_table) return -1;
    
    size_t num_pages = (size + RV32_PGSIZE - 1) / RV32_PGSIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint32_t vpn = (vaddr + i * RV32_PGSIZE) >> RV32_PGSHIFT;
        root_page_table[vpn] = 0;
    }
    
    return 0;
}

void hal_mmu_enable(void) {
    uint32_t satp = ((uintptr_t)root_page_table >> RV32_PGSHIFT) | (1u << 31);
    __asm__ volatile("csrw satp, %0" :: "r"(satp));
    __asm__ volatile("sfence.vma");
}
