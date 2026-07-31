/* tach - Small fixed-size object caches layered on kmalloc. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/kheap.h>
#include <mm/slab.h>

#define SLAB_BYTES 4096u

struct slab_page {
    struct slab_page* next;
    void* free_list;
    size_t in_use;
    size_t capacity;
    size_t bytes;
};

static spinlock_t g_slab_lock;
static bool g_slab_ready;

static size_t slab_align(size_t value) {
    size_t alignment = sizeof(uintptr_t);
    return (value + alignment - 1) & ~(alignment - 1);
}

/* Freestanding ARM32 does not provide the compiler's __aeabi_uidiv
 * helpers, so keep slab arithmetic self-contained. */
static size_t slab_divide(size_t dividend, size_t divisor,
                          size_t* remainder_out) {
    size_t quotient = 0;
    size_t remainder = 0;
    if (!divisor) return 0;
    for (int bit = (int)(sizeof(size_t) * 8) - 1; bit >= 0; bit--) {
        remainder = (remainder << 1) | ((dividend >> bit) & 1u);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (size_t)1 << bit;
        }
    }
    if (remainder_out) *remainder_out = remainder;
    return quotient;
}

void slab_init(void) {
    spinlock_init(&g_slab_lock);
    g_slab_ready = true;
}

struct slab_cache* slab_cache_create(const char* name, size_t size) {
    if (!size) return NULL;
    if (!g_slab_ready) slab_init();
    struct slab_cache* cache = (struct slab_cache*)kzalloc(sizeof(*cache));
    if (!cache) return NULL;
    cache->name = name;
    cache->object_size = slab_align(size < sizeof(void*) ? sizeof(void*) : size);
    size_t available = SLAB_BYTES - slab_align(sizeof(struct slab_page));
    cache->objects_per_slab = slab_divide(available, cache->object_size, NULL);
    if (!cache->objects_per_slab) cache->objects_per_slab = 1;
    return cache;
}

static struct slab_page* slab_page_create(struct slab_cache* cache) {
    size_t header = slab_align(sizeof(struct slab_page));
    size_t bytes = header + cache->objects_per_slab * cache->object_size;
    struct slab_page* page = (struct slab_page*)kmalloc(bytes);
    if (!page) return NULL;
    memset(page, 0, header);
    page->capacity = cache->objects_per_slab;
    page->bytes = bytes;
    uint8_t* objects = (uint8_t*)page + header;
    for (size_t i = 0; i < page->capacity; i++) {
        void* object = objects + i * cache->object_size;
        *(void**)object = page->free_list;
        page->free_list = object;
    }
    page->next = (struct slab_page*)cache->slabs;
    cache->slabs = page;
    return page;
}

void* slab_alloc(struct slab_cache* cache) {
    if (!cache) return NULL;
    spinlock_lock(&g_slab_lock);
    struct slab_page* page = (struct slab_page*)cache->slabs;
    while (page && !page->free_list) page = page->next;
    if (!page) {
        spinlock_unlock(&g_slab_lock);
        page = slab_page_create(cache);
        if (!page) return NULL;
        spinlock_lock(&g_slab_lock);
    }
    void* object = page->free_list;
    page->free_list = *(void**)object;
    page->in_use++;
    cache->allocation_count++;
    spinlock_unlock(&g_slab_lock);
    memset(object, 0, cache->object_size);
    return object;
}

void slab_free(struct slab_cache* cache, void* object) {
    if (!cache || !object) return;
    spinlock_lock(&g_slab_lock);
    size_t header = slab_align(sizeof(struct slab_page));
    for (struct slab_page* page = (struct slab_page*)cache->slabs;
         page; page = page->next) {
        uintptr_t first = (uintptr_t)page + header;
        uintptr_t end = first + page->capacity * cache->object_size;
        uintptr_t address = (uintptr_t)object;
        size_t remainder = 0;
        if (address >= first && address < end)
            (void)slab_divide(address - first, cache->object_size, &remainder);
        if (address < first || address >= end || remainder != 0) continue;
        *(void**)object = page->free_list;
        page->free_list = object;
        if (page->in_use) page->in_use--;
        if (cache->allocation_count) cache->allocation_count--;
        spinlock_unlock(&g_slab_lock);
        return;
    }
    spinlock_unlock(&g_slab_lock);
}

void slab_cache_destroy(struct slab_cache* cache) {
    if (!cache) return;
    spinlock_lock(&g_slab_lock);
    if (cache->allocation_count) {
        spinlock_unlock(&g_slab_lock);
        return;
    }
    struct slab_page* pages = (struct slab_page*)cache->slabs;
    cache->slabs = NULL;
    spinlock_unlock(&g_slab_lock);
    while (pages) {
        struct slab_page* next = pages->next;
        kfree(pages);
        pages = next;
    }
    kfree(cache);
}
