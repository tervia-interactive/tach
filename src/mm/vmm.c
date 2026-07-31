/* tach - Virtual Memory Manager */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <hal/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define VMM_MAX_CONTEXTS 32
#define VMM_MAX_MAPPINGS 512

#if UINTPTR_MAX > UINT32_MAX
#define VMM_DYNAMIC_BASE ((uintptr_t)0x0000004000000000ULL)
#else
#define VMM_DYNAMIC_BASE ((uintptr_t)0x40000000UL)
#endif

struct vmm_mapping {
    uintptr_t virt;
    phys_addr_t phys;
    uint32_t flags;
    bool owned;
};

struct vmm_slot {
    bool used;
    struct vmm_context context;
    struct vmm_mapping mappings[VMM_MAX_MAPPINGS];
    size_t mapping_count;
};

static struct vmm_context g_kernel_context;
static struct vmm_slot g_slots[VMM_MAX_CONTEXTS];
static struct vmm_context* g_current_context;
static spinlock_t g_vmm_lock;

static struct vmm_slot* slot_for(struct vmm_context* context) {
    for (size_t i = 0; i < VMM_MAX_CONTEXTS; i++) {
        if (g_slots[i].used && &g_slots[i].context == context) {
            return &g_slots[i];
        }
    }
    return NULL;
}

static struct vmm_mapping* find_mapping(struct vmm_slot* slot,
                                        uintptr_t virt) {
    if (!slot) {
        return NULL;
    }
    virt &= ~(uintptr_t)(PAGE_SIZE - 1);
    for (size_t i = 0; i < slot->mapping_count; i++) {
        if (slot->mappings[i].virt == virt) {
            return &slot->mappings[i];
        }
    }
    return NULL;
}

void vmm_init(void) {
    spinlock_init(&g_vmm_lock);
    memset(g_slots, 0, sizeof(g_slots));
    memset(&g_kernel_context, 0, sizeof(g_kernel_context));
    if (arch_paging_init() == 0) {
        g_kernel_context.page_table = arch_paging_kernel_root();
    }
    g_kernel_context.next_free = VMM_DYNAMIC_BASE;
    g_current_context = &g_kernel_context;
}

struct vmm_context* vmm_kernel_context(void) {
    return &g_kernel_context;
}

struct vmm_context* vmm_create_context(void) {
    spinlock_lock(&g_vmm_lock);
    for (size_t i = 0; i < VMM_MAX_CONTEXTS; i++) {
        if (g_slots[i].used) {
            continue;
        }
        void* root = arch_paging_create_root();
        if (!root) {
            spinlock_unlock(&g_vmm_lock);
            return NULL;
        }
        memset(&g_slots[i], 0, sizeof(g_slots[i]));
        g_slots[i].used = true;
        g_slots[i].context.page_table = root;
        g_slots[i].context.next_free = VMM_DYNAMIC_BASE;
        spinlock_unlock(&g_vmm_lock);
        return &g_slots[i].context;
    }
    spinlock_unlock(&g_vmm_lock);
    return NULL;
}

void vmm_destroy_context(struct vmm_context* context) {
    struct vmm_slot* slot = slot_for(context);
    if (!slot || context == g_current_context) {
        return;
    }
    for (size_t i = 0; i < slot->mapping_count; i++) {
        arch_paging_unmap(context->page_table, slot->mappings[i].virt);
        if (slot->mappings[i].owned) {
            pmm_free_frame(slot->mappings[i].phys);
        }
    }
    arch_paging_destroy_root(context->page_table);
    memset(slot, 0, sizeof(*slot));
}

void vmm_switch_context(struct vmm_context* context) {
    if (!context) {
        context = &g_kernel_context;
    }
    if (context == g_current_context || !context->page_table) {
        return;
    }
    arch_paging_switch(context->page_table);
    g_current_context = context;
}

int vmm_map(struct vmm_context* context, void* virtual_address,
            phys_addr_t physical_address, uint32_t flags) {
    if (!context || !context->page_table || !virtual_address ||
        ((uintptr_t)virtual_address & (PAGE_SIZE - 1)) ||
        (physical_address & (PAGE_SIZE - 1))) {
        return -EINVAL;
    }
    struct vmm_slot* slot = slot_for(context);
    struct vmm_mapping* mapping = find_mapping(slot,
                                               (uintptr_t)virtual_address);
    if (!mapping && slot && slot->mapping_count >= VMM_MAX_MAPPINGS) {
        return -ENOSPC;
    }
    int result = arch_paging_map(context->page_table,
                                 (uintptr_t)virtual_address,
                                 physical_address, flags | VMM_PRESENT);
    if (result < 0) {
        return result;
    }
    if (slot) {
        if (!mapping) {
            mapping = &slot->mappings[slot->mapping_count++];
            memset(mapping, 0, sizeof(*mapping));
            mapping->virt = (uintptr_t)virtual_address;
        }
        mapping->phys = physical_address;
        mapping->flags = flags | VMM_PRESENT;
    }
    return 0;
}

int vmm_map_allocated(struct vmm_context* context, void* virtual_address,
                      uint32_t flags, phys_addr_t* physical_out) {
    phys_addr_t physical = pmm_alloc_frame();
    if (!physical) {
        return -ENOMEM;
    }
    memset((void*)(uintptr_t)physical, 0, PAGE_SIZE);
    int result = vmm_map(context, virtual_address, physical, flags);
    if (result < 0) {
        pmm_free_frame(physical);
        return result;
    }
    struct vmm_mapping* mapping =
        find_mapping(slot_for(context), (uintptr_t)virtual_address);
    if (mapping) {
        mapping->owned = true;
    }
    if (physical_out) {
        *physical_out = physical;
    }
    return 0;
}

int vmm_unmap(struct vmm_context* context, void* virtual_address) {
    if (!context || !virtual_address ||
        ((uintptr_t)virtual_address & (PAGE_SIZE - 1))) {
        return -EINVAL;
    }
    int result = arch_paging_unmap(context->page_table,
                                   (uintptr_t)virtual_address);
    if (result < 0) {
        return result;
    }
    struct vmm_slot* slot = slot_for(context);
    if (!slot) {
        return 0;
    }
    for (size_t i = 0; i < slot->mapping_count; i++) {
        if (slot->mappings[i].virt != (uintptr_t)virtual_address) {
            continue;
        }
        if (slot->mappings[i].owned) {
            pmm_free_frame(slot->mappings[i].phys);
        }
        slot->mappings[i] = slot->mappings[--slot->mapping_count];
        return 0;
    }
    return 0;
}

void* vmm_alloc_page(struct vmm_context* context, uint32_t flags) {
    if (!context) {
        return NULL;
    }
    uintptr_t virtual_address =
        (context->next_free + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    while (vmm_resolve(context, (void*)virtual_address)) {
        virtual_address += PAGE_SIZE;
    }
    if (vmm_map_allocated(context, (void*)virtual_address, flags, NULL) < 0) {
        return NULL;
    }
    context->next_free = virtual_address + PAGE_SIZE;
    return (void*)virtual_address;
}

void vmm_free_page(struct vmm_context* context, void* virtual_address) {
    (void)vmm_unmap(context, virtual_address);
}

phys_addr_t vmm_resolve(struct vmm_context* context, const void* address) {
    struct vmm_mapping* mapping =
        find_mapping(slot_for(context), (uintptr_t)address);
    if (!mapping) {
        return 0;
    }
    return mapping->phys + ((uintptr_t)address & (PAGE_SIZE - 1));
}

int vmm_handle_fault(uintptr_t address, int is_write) {
    (void)address;
    (void)is_write;
    return -EFAULT;
}
