/* tach - HAL memory-map and legacy MMU compatibility */
#include <kernel/types.h>
#include <kernel/compiler.h>
#include <kernel/string.h>
#include <hal/mm.h>

#define HAL_MAX_MEMORY_REGIONS 64

static mem_region_t g_regions[HAL_MAX_MEMORY_REGIONS];
static int g_region_count;

WEAK void hal_mmu_init(void) {}

WEAK int hal_mmu_map(uintptr_t virt, uintptr_t phys, size_t size,
                     uint32_t flags) {
    (void)virt;
    (void)phys;
    (void)size;
    (void)flags;
    return -1;
}

WEAK int hal_mmu_unmap(uintptr_t virt, size_t size) {
    (void)virt;
    (void)size;
    return -1;
}

void hal_memmap_reset(void) {
    memset(g_regions, 0, sizeof(g_regions));
    g_region_count = 0;
}

int hal_memmap_add(uint64_t base, uint64_t size, uint32_t type) {
    if (!size || g_region_count >= HAL_MAX_MEMORY_REGIONS) {
        return -1;
    }
    if (base + size < base) {
        size = UINT64_MAX - base;
    }
    g_regions[g_region_count].base = base;
    g_regions[g_region_count].size = size;
    g_regions[g_region_count].type = type;
    g_region_count++;
    return 0;
}

void hal_memmap_use_platform_fallback(void) {
    hal_memmap_reset();
#if defined(__x86_64__) || defined(__i386__)
    hal_memmap_add(0, 0x100000, MEM_REGION_RESERVED);
    hal_memmap_add(0x100000, 0x0ff00000, MEM_REGION_USABLE);
#elif defined(__aarch64__)
    hal_memmap_add(0, 0x40000000, MEM_REGION_RESERVED);
    hal_memmap_add(0x40000000, 0x10000000, MEM_REGION_USABLE);
#elif defined(__arm__)
    hal_memmap_add(0, 0x40000000, MEM_REGION_RESERVED);
    hal_memmap_add(0x40000000, 0x08000000, MEM_REGION_USABLE);
#elif defined(__riscv)
    hal_memmap_add(0x80000000ULL, 0x00200000ULL, MEM_REGION_RESERVED);
    hal_memmap_add(0x80200000ULL, 0x07e00000ULL, MEM_REGION_USABLE);
#else
    hal_memmap_add(0x100000, 0x03f00000, MEM_REGION_USABLE);
#endif
}

int hal_get_memmap(mem_region_t* regions, int max_regions) {
    if (!regions || max_regions <= 0) {
        return g_region_count;
    }
    int count = g_region_count < max_regions ? g_region_count : max_regions;
    memcpy(regions, g_regions, (size_t)count * sizeof(*regions));
    return count;
}
