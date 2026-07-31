#include <kernel/string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/slab.h>

#define CHECK(expression) do { if (!(expression)) return __LINE__; } while (0)

static uint8_t g_arena[64 * 1024] __attribute__((aligned(16)));
static uint8_t g_pmm_arena[128 * 1024] __attribute__((aligned(PAGE_SIZE)));
static bool g_pmm_used;
void* pmm_alloc_pages(size_t count) {
    if (g_pmm_used || count * PAGE_SIZE > sizeof(g_pmm_arena)) return NULL;
    g_pmm_used = true;
    return g_pmm_arena;
}
void pmm_free_pages(void* pages, size_t count) {
    (void)count;
    if (pages == g_pmm_arena) g_pmm_used = false;
}

int main(void) {
    kheap_init(g_arena, sizeof(g_arena));
    uint8_t* first = (uint8_t*)kmalloc(37);
    CHECK(first != NULL);
    CHECK(((uintptr_t)first & 15u) == 0);
    memset(first, 0x5a, 37);
    uint8_t* zero = (uint8_t*)kzalloc(200);
    CHECK(zero != NULL);
    for (size_t i = 0; i < 200; i++) CHECK(zero[i] == 0);
    first = (uint8_t*)krealloc(first, 600);
    CHECK(first != NULL);
    for (size_t i = 0; i < 37; i++) CHECK(first[i] == 0x5a);
    kfree(first);
    kfree(zero);
    CHECK(kmalloc(32 * 1024) != NULL);
    void* large = kmalloc(80 * 1024);
    CHECK(large != NULL);
    CHECK(g_pmm_used);
    kfree(large);
    CHECK(!g_pmm_used);

    slab_init();
    struct slab_cache* cache = slab_cache_create("test", 24);
    CHECK(cache != NULL);
    void* objects[96];
    for (size_t i = 0; i < 96; i++) {
        objects[i] = slab_alloc(cache);
        CHECK(objects[i] != NULL);
    }
    CHECK(cache->allocation_count == 96);
    for (size_t i = 0; i < 96; i++) slab_free(cache, objects[i]);
    CHECK(cache->allocation_count == 0);
    slab_cache_destroy(cache);
    return 0;
}
