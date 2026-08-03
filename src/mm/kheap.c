/* tach - Kernel heap backed by PMM pages or a caller-provided arena. */
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/kheap.h>
#include <mm/pmm.h>

#define KHEAP_MAGIC 0x74616368u
#define KHEAP_ALIGN 16u
#define KHEAP_FROM_PMM 1u

struct heap_block {
    uint32_t magic;
    uint32_t flags;
    size_t size;
    size_t pages;
    bool free;
    struct heap_block* previous;
    struct heap_block* next;
};

static struct heap_block* g_blocks;
static spinlock_t g_heap_lock;
static bool g_heap_ready;

static size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static size_t header_size(void) {
    return align_up(sizeof(struct heap_block), KHEAP_ALIGN);
}

static void append_block(struct heap_block* block) {
    block->previous = NULL;
    block->next = g_blocks;
    if (g_blocks) g_blocks->previous = block;
    g_blocks = block;
}

static void remove_block(struct heap_block* block) {
    if (block->previous) block->previous->next = block->next;
    else g_blocks = block->next;
    if (block->next) block->next->previous = block->previous;
}

void kheap_init(void* start, size_t size) {
    spinlock_init(&g_heap_lock);
    g_blocks = NULL;
    g_heap_ready = true;
    if (!start || size <= header_size() + KHEAP_ALIGN) return;

    uintptr_t base = ((uintptr_t)start + KHEAP_ALIGN - 1) &
                     ~(uintptr_t)(KHEAP_ALIGN - 1);
    size_t skipped = base - (uintptr_t)start;
    if (skipped >= size || size - skipped <= header_size()) return;
    struct heap_block* block = (struct heap_block*)base;
    memset(block, 0, sizeof(*block));
    block->magic = KHEAP_MAGIC;
    block->size = size - skipped - header_size();
    block->free = true;
    append_block(block);
}

static void split_arena_block(struct heap_block* block, size_t wanted) {
    size_t overhead = header_size();
    if ((block->flags & KHEAP_FROM_PMM) ||
        block->size < wanted + overhead + KHEAP_ALIGN) return;
    struct heap_block* remainder = (struct heap_block*)
        ((uint8_t*)block + overhead + wanted);
    memset(remainder, 0, sizeof(*remainder));
    remainder->magic = KHEAP_MAGIC;
    remainder->size = block->size - wanted - overhead;
    remainder->free = true;
    remainder->previous = block;
    remainder->next = block->next;
    if (remainder->next) remainder->next->previous = remainder;
    block->next = remainder;
    block->size = wanted;
}

void* kmalloc(size_t size) {
    if (!size) return NULL;
    if (!g_heap_ready) kheap_init(NULL, 0);
    size = align_up(size, KHEAP_ALIGN);

    spinlock_lock(&g_heap_lock);
    for (struct heap_block* block = g_blocks; block; block = block->next) {
        if (!block->free || block->size < size) continue;
        split_arena_block(block, size);
        block->free = false;
        void* result = (uint8_t*)block + header_size();
        spinlock_unlock(&g_heap_lock);
        return result;
    }
    spinlock_unlock(&g_heap_lock);

    size_t bytes = header_size() + size;
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    struct heap_block* block = (struct heap_block*)pmm_alloc_pages(pages);
    if (!block) return NULL;
    memset(block, 0, header_size());
    block->magic = KHEAP_MAGIC;
    block->flags = KHEAP_FROM_PMM;
    block->size = pages * PAGE_SIZE - header_size();
    block->pages = pages;

    spinlock_lock(&g_heap_lock);
    append_block(block);
    spinlock_unlock(&g_heap_lock);
    return (uint8_t*)block + header_size();
}

void* kzalloc(size_t size) {
    void* allocation = kmalloc(size);
    if (allocation) memset(allocation, 0, size);
    return allocation;
}

static void merge_arena_neighbors(struct heap_block* block) {
    size_t overhead = header_size();
    if (block->next && block->next->free &&
        !(block->next->flags & KHEAP_FROM_PMM) &&
        (uint8_t*)block + overhead + block->size ==
            (uint8_t*)block->next) {
        struct heap_block* next = block->next;
        block->size += overhead + next->size;
        block->next = next->next;
        if (block->next) block->next->previous = block;
    }
    if (block->previous && block->previous->free &&
        !(block->previous->flags & KHEAP_FROM_PMM)) {
        struct heap_block* previous = block->previous;
        if ((uint8_t*)previous + overhead + previous->size ==
            (uint8_t*)block) {
            previous->size += overhead + block->size;
            previous->next = block->next;
            if (previous->next) previous->next->previous = previous;
        }
    }
}

void kfree(void* pointer) {
    if (!pointer) return;
    struct heap_block* block = (struct heap_block*)
        ((uint8_t*)pointer - header_size());
    if (block->magic != KHEAP_MAGIC) return;

    spinlock_lock(&g_heap_lock);
    if (block->free) {
        spinlock_unlock(&g_heap_lock);
        return;
    }
    if (block->flags & KHEAP_FROM_PMM) {
        size_t pages = block->pages;
        remove_block(block);
        block->magic = 0;
        spinlock_unlock(&g_heap_lock);
        pmm_free_pages(block, pages);
        return;
    }
    block->free = true;
    merge_arena_neighbors(block);
    spinlock_unlock(&g_heap_lock);
}

void* krealloc(void* pointer, size_t new_size) {
    if (!pointer) return kmalloc(new_size);
    if (!new_size) {
        kfree(pointer);
        return NULL;
    }
    struct heap_block* block = (struct heap_block*)
        ((uint8_t*)pointer - header_size());
    if (block->magic != KHEAP_MAGIC) return NULL;
    if (block->size >= new_size) return pointer;

    void* replacement = kmalloc(new_size);
    if (!replacement) return NULL;
    memcpy(replacement, pointer, block->size);
    kfree(pointer);
    return replacement;
}
