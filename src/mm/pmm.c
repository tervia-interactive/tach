#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/pmm.h>

#define PMM_MAX_PHYS_ADDR (1ULL << 32)
#define PMM_MAX_FRAMES ((size_t)(PMM_MAX_PHYS_ADDR / PAGE_SIZE))
#define PMM_BITMAP_BYTES (PMM_MAX_FRAMES / 8)

static uint8_t g_frame_bitmap[PMM_BITMAP_BYTES];
static uint16_t g_frame_refs[PMM_MAX_FRAMES];
static size_t g_frame_limit;
static size_t g_total_pages;
static size_t g_free_pages;
static size_t g_search_start;
static spinlock_t g_pmm_lock;

#ifndef TACH_HOST_TEST
extern char __kernel_start[];
extern char __kernel_end[];
#endif

static bool frame_is_used(size_t frame) {
    return (g_frame_bitmap[frame >> 3] & (uint8_t)(1u << (frame & 7))) != 0;
}

static void frame_set_used(size_t frame) {
    uint8_t mask = (uint8_t)(1u << (frame & 7));
    if (!(g_frame_bitmap[frame >> 3] & mask)) {
        g_frame_bitmap[frame >> 3] |= mask;
        if (g_free_pages) {
            g_free_pages--;
        }
    }
    if (!g_frame_refs[frame]) g_frame_refs[frame] = 1;
}

static void frame_set_free(size_t frame) {
    uint8_t mask = (uint8_t)(1u << (frame & 7));
    if (g_frame_bitmap[frame >> 3] & mask) {
        g_frame_bitmap[frame >> 3] &= (uint8_t)~mask;
        g_free_pages++;
    }
    g_frame_refs[frame] = 0;
}

static size_t region_first_frame(uint64_t start) {
    return (size_t)((start + PAGE_SIZE - 1) >> PAGE_SHIFT);
}

static size_t region_last_frame(uint64_t start, uint64_t size) {
    uint64_t end = start + size;
    if (end < start || end > PMM_MAX_PHYS_ADDR) {
        end = PMM_MAX_PHYS_ADDR;
    }
    return (size_t)(end >> PAGE_SHIFT);
}

void pmm_mark_region_free(phys_addr_t start, size_t size) {
    size_t first = region_first_frame(start);
    size_t last = region_last_frame(start, size);
    if (last > g_frame_limit) {
        last = g_frame_limit;
    }
    for (size_t frame = first; frame < last; frame++) {
        frame_set_free(frame);
    }
}

void pmm_mark_region_used(phys_addr_t start, size_t size) {
    size_t first = (size_t)((uint64_t)start >> PAGE_SHIFT);
    uint64_t end = (uint64_t)start + size;
    if (end < (uint64_t)start || end > PMM_MAX_PHYS_ADDR) {
        end = PMM_MAX_PHYS_ADDR;
    }
    size_t last = (size_t)((end + PAGE_SIZE - 1) >> PAGE_SHIFT);
    if (last > g_frame_limit) {
        last = g_frame_limit;
    }
    for (size_t frame = first; frame < last; frame++) {
        frame_set_used(frame);
    }
}

void pmm_init(const mem_region_t* memory_map, size_t region_count) {
    spinlock_init(&g_pmm_lock);
    memset(g_frame_bitmap, 0xff, sizeof(g_frame_bitmap));
    memset(g_frame_refs, 0, sizeof(g_frame_refs));
    g_frame_limit = 0;
    g_total_pages = 0;
    g_free_pages = 0;
    g_search_start = 1;

    for (size_t i = 0; i < region_count; i++) {
        uint64_t end = memory_map[i].base + memory_map[i].size;
        if (end < memory_map[i].base || end > PMM_MAX_PHYS_ADDR) {
            end = PMM_MAX_PHYS_ADDR;
        }
        size_t frame_end = (size_t)(end >> PAGE_SHIFT);
        if (frame_end > g_frame_limit) {
            g_frame_limit = frame_end;
        }
    }
    if (g_frame_limit > PMM_MAX_FRAMES) {
        g_frame_limit = PMM_MAX_FRAMES;
    }

    for (size_t i = 0; i < region_count; i++) {
        if (memory_map[i].type == MEM_REGION_USABLE) {
            pmm_mark_region_free((phys_addr_t)memory_map[i].base,
                                 (size_t)memory_map[i].size);
        }
    }
    for (size_t i = 0; i < region_count; i++) {
        if (memory_map[i].type != MEM_REGION_USABLE) {
            pmm_mark_region_used((phys_addr_t)memory_map[i].base,
                                 (size_t)memory_map[i].size);
        }
    }

    pmm_mark_region_used(0, PAGE_SIZE);
#ifndef TACH_HOST_TEST
    pmm_mark_region_used((phys_addr_t)(uintptr_t)__kernel_start,
                         (size_t)(__kernel_end - __kernel_start));
#endif
    g_total_pages = g_free_pages;
}

phys_addr_t pmm_alloc_frame(void) {
    spinlock_lock(&g_pmm_lock);
    for (size_t pass = 0; pass < 2; pass++) {
        size_t begin = pass == 0 ? g_search_start : 1;
        size_t end = pass == 0 ? g_frame_limit : g_search_start;
        for (size_t frame = begin; frame < end; frame++) {
            if (!frame_is_used(frame)) {
                frame_set_used(frame);
                g_frame_refs[frame] = 1;
                g_search_start = frame + 1;
                spinlock_unlock(&g_pmm_lock);
                return (phys_addr_t)(frame << PAGE_SHIFT);
            }
        }
    }
    spinlock_unlock(&g_pmm_lock);
    return 0;
}

bool pmm_retain_frame(phys_addr_t frame_address) {
    if (!frame_address || (frame_address & (PAGE_SIZE - 1))) return false;
    size_t frame = (size_t)(frame_address >> PAGE_SHIFT);
    if (frame >= g_frame_limit) return false;
    spinlock_lock(&g_pmm_lock);
    bool retained = frame_is_used(frame) && g_frame_refs[frame] &&
                    g_frame_refs[frame] != 0xffffu;
    if (retained) g_frame_refs[frame]++;
    spinlock_unlock(&g_pmm_lock);
    return retained;
}

void pmm_free_frame(phys_addr_t frame_address) {
    if (!frame_address || (frame_address & (PAGE_SIZE - 1))) {
        return;
    }
    size_t frame = (size_t)(frame_address >> PAGE_SHIFT);
    if (frame >= g_frame_limit) {
        return;
    }
    spinlock_lock(&g_pmm_lock);
    if (frame_is_used(frame) && g_frame_refs[frame]) {
        if (--g_frame_refs[frame] == 0) {
            frame_set_free(frame);
            if (frame < g_search_start) g_search_start = frame;
        }
    }
    spinlock_unlock(&g_pmm_lock);
}

size_t pmm_frame_refcount(phys_addr_t frame_address) {
    if (!frame_address || (frame_address & (PAGE_SIZE - 1))) return 0;
    size_t frame = (size_t)(frame_address >> PAGE_SHIFT);
    if (frame >= g_frame_limit) return 0;
    spinlock_lock(&g_pmm_lock);
    size_t refs = g_frame_refs[frame];
    spinlock_unlock(&g_pmm_lock);
    return refs;
}

void* pmm_alloc_page(void) {
    return (void*)(uintptr_t)pmm_alloc_frame();
}

void pmm_free_page(void* page) {
    pmm_free_frame((phys_addr_t)(uintptr_t)page);
}

void* pmm_alloc_aligned_pages(size_t count, size_t alignment_pages) {
    if (!count || !alignment_pages) {
        return NULL;
    }
    spinlock_lock(&g_pmm_lock);
    size_t run = 0;
    size_t first = 0;
    for (size_t frame = 1; frame < g_frame_limit; frame++) {
        if (!run && (frame & (alignment_pages - 1)) != 0) {
            continue;
        }
        if (!frame_is_used(frame)) {
            if (!run) {
                first = frame;
            }
            if (++run == count) {
                for (size_t i = 0; i < count; i++) {
                    frame_set_used(first + i);
                    g_frame_refs[first + i] = 1;
                }
                g_search_start = first + count;
                spinlock_unlock(&g_pmm_lock);
                return (void*)(uintptr_t)(first << PAGE_SHIFT);
            }
        } else {
            run = 0;
        }
    }
    spinlock_unlock(&g_pmm_lock);
    return NULL;
}

void* pmm_alloc_pages(size_t count) {
    return pmm_alloc_aligned_pages(count, 1);
}

void pmm_free_pages(void* pages, size_t count) {
    phys_addr_t base = (phys_addr_t)(uintptr_t)pages;
    for (size_t i = 0; i < count; i++) {
        pmm_free_frame(base + i * PAGE_SIZE);
    }
}

size_t pmm_get_free_pages(void) {
    return g_free_pages;
}

size_t pmm_get_total_pages(void) {
    return g_total_pages;
}
