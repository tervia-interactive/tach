/* tach - Virtual Memory Manager */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <hal/paging.h>
#include <hal/smp.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <kernel/percpu.h>

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
static struct vmm_context* g_current_context[MAX_CPUS];
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
    for (size_t cpu = 0; cpu < MAX_CPUS; cpu++)
        g_current_context[cpu] = &g_kernel_context;
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

struct vmm_context* vmm_clone_context(struct vmm_context* source) {
    if (!source) return NULL;
    struct vmm_context* clone = vmm_create_context();
    if (!clone) return NULL;
    struct vmm_slot* source_slot = slot_for(source);
    if (!source_slot) return clone;
    uint32_t original_flags[VMM_MAX_MAPPINGS];
    size_t source_count = source_slot->mapping_count;
    for (size_t i = 0; i < source_count; i++)
        original_flags[i] = source_slot->mappings[i].flags;
    for (size_t i = 0; i < source_count; i++) {
        struct vmm_mapping* mapping = &source_slot->mappings[i];
        uint32_t old_flags = mapping->flags;
        uint32_t shared_flags = old_flags;
        if (mapping->owned && (old_flags & (VMM_WRITABLE | VMM_COW))) {
            shared_flags = (old_flags & ~VMM_WRITABLE) | VMM_COW;
            int protected = arch_paging_map(source->page_table, mapping->virt,
                                            mapping->phys, shared_flags);
            if (protected < 0) goto rollback;
            mapping->flags = shared_flags | VMM_PRESENT;
        }
        if (mapping->owned && !pmm_retain_frame(mapping->phys)) {
            goto rollback;
        }
        int result = vmm_map(clone, (void*)mapping->virt, mapping->phys,
                             shared_flags);
        if (result < 0) {
            if (mapping->owned) pmm_free_frame(mapping->phys);
            goto rollback;
        }
        struct vmm_mapping* child_mapping =
            find_mapping(slot_for(clone), mapping->virt);
        if (child_mapping) child_mapping->owned = mapping->owned;
    }
    clone->next_free = source->next_free;
    return clone;

rollback:
    for (size_t i = 0; i < source_count; i++) {
        struct vmm_mapping* mapping = &source_slot->mappings[i];
        if (mapping->flags == original_flags[i]) continue;
        (void)arch_paging_map(source->page_table, mapping->virt,
                              mapping->phys, original_flags[i]);
        mapping->flags = original_flags[i];
    }
    vmm_destroy_context(clone);
    return NULL;
}

static int break_cow(struct vmm_context* context, uintptr_t page) {
    struct vmm_mapping* mapping = find_mapping(slot_for(context), page);
    if (!mapping || !(mapping->flags & VMM_COW) || !mapping->owned)
        return -EACCES;

    uint32_t writable = (mapping->flags | VMM_WRITABLE) & ~VMM_COW;
    if (pmm_frame_refcount(mapping->phys) == 1) {
        int result = arch_paging_map(context->page_table, page,
                                     mapping->phys, writable);
        if (result < 0) return result;
        mapping->flags = writable | VMM_PRESENT;
        return 0;
    }

    phys_addr_t replacement = pmm_alloc_frame();
    if (!replacement) return -ENOMEM;
    memcpy((void*)(uintptr_t)replacement,
           (const void*)(uintptr_t)mapping->phys, PAGE_SIZE);
    int result = arch_paging_map(context->page_table, page, replacement,
                                 writable);
    if (result < 0) {
        pmm_free_frame(replacement);
        return result;
    }
    phys_addr_t shared = mapping->phys;
    mapping->phys = replacement;
    mapping->flags = writable | VMM_PRESENT;
    pmm_free_frame(shared);
    return 0;
}

void vmm_destroy_context(struct vmm_context* context) {
    struct vmm_slot* slot = slot_for(context);
    if (!slot) {
        return;
    }
    for (size_t cpu = 0; cpu < MAX_CPUS; cpu++)
        if (context == g_current_context[cpu]) return;
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
    uint32_t cpu = hal_smp_current_cpu();
    if (context == g_current_context[cpu] || !context->page_table) {
        return;
    }
    arch_paging_switch(context->page_table);
    g_current_context[cpu] = context;
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

uint32_t vmm_get_flags(struct vmm_context* context, const void* address) {
    struct vmm_mapping* mapping =
        find_mapping(slot_for(context), (uintptr_t)address);
    return mapping ? mapping->flags : 0;
}

int vmm_protect(struct vmm_context* context, void* address, uint32_t flags) {
    if (!context || !address) return -EINVAL;
    uintptr_t page = (uintptr_t)address & ~(uintptr_t)(PAGE_SIZE - 1);
    struct vmm_mapping* mapping = find_mapping(slot_for(context), page);
    if (!mapping) return -ENOENT;
    int result = arch_paging_map(context->page_table, page,
                                 mapping->phys, flags | VMM_PRESENT);
    if (result < 0) return result;
    mapping->flags = flags | VMM_PRESENT;
    return 0;
}

static int copy_user_range(void* destination, struct vmm_context* context,
                           const void* source, size_t size, bool to_user) {
    if (!context || (!destination && size) || (!source && size)) return -EFAULT;
    uint8_t* output = (uint8_t*)destination;
    const uint8_t* input = (const uint8_t*)source;
    while (size) {
        uintptr_t user_address = to_user ? (uintptr_t)output :
                                           (uintptr_t)input;
        uint32_t flags = vmm_get_flags(context, (void*)user_address);
        if (to_user && (flags & VMM_COW)) {
            int result = break_cow(context,
                user_address & ~(uintptr_t)(PAGE_SIZE - 1));
            if (result < 0) return result;
            flags = vmm_get_flags(context, (void*)user_address);
        }
        if (!(flags & VMM_USER) || (to_user && !(flags & VMM_WRITABLE)))
            return -EFAULT;
        phys_addr_t physical = vmm_resolve(context, (void*)user_address);
        if (!physical) return -EFAULT;
        size_t chunk = PAGE_SIZE - (user_address & (PAGE_SIZE - 1));
        if (chunk > size) chunk = size;
        if (to_user) memcpy((void*)(uintptr_t)physical, input, chunk);
        else memcpy(output, (const void*)(uintptr_t)physical, chunk);
        output += chunk;
        input += chunk;
        size -= chunk;
    }
    return 0;
}

int vmm_copy_from_user(void* destination, struct vmm_context* context,
                       const void* source, size_t size) {
    return copy_user_range(destination, context, source, size, false);
}

int vmm_copy_to_user(struct vmm_context* context, void* destination,
                     const void* source, size_t size) {
    return copy_user_range(destination, context, source, size, true);
}

int vmm_copy_string_from_user(char* destination, size_t capacity,
                              struct vmm_context* context,
                              const char* source) {
    if (!destination || !capacity || !source) return -EFAULT;
    for (size_t i = 0; i < capacity; i++) {
        int result = vmm_copy_from_user(&destination[i], context,
                                        source + i, 1);
        if (result < 0) return result;
        if (!destination[i]) return 0;
    }
    destination[capacity - 1] = '\0';
    return -ENAMETOOLONG;
}

int vmm_handle_fault(uintptr_t address, int is_write) {
    struct process* process = process_get_current();
    if (!process || !process->user_mode || !process->mm) return -EFAULT;
    uintptr_t page = address & ~(uintptr_t)(PAGE_SIZE - 1);
    if (vmm_resolve(process->mm, (void*)page)) {
        uint32_t flags = vmm_get_flags(process->mm, (void*)page);
        if (is_write && (flags & VMM_COW))
            return break_cow(process->mm, page);
        if (is_write && !(flags & VMM_WRITABLE)) return -EACCES;
        return 0;
    }

    uintptr_t stack_limit = process->user_stack_top - PROCESS_USER_STACK_MAX;
    if (page < process->user_stack_bottom && page >= stack_limit &&
        page + PAGE_SIZE == process->user_stack_bottom) {
        int result = vmm_map_allocated(process->mm, (void*)page,
                                       VMM_USER | VMM_WRITABLE, NULL);
        if (result < 0) return result;
        process->user_stack_bottom = page;
        return 0;
    }
    if (page >= process->brk_start && address < process->brk_end) {
        return vmm_map_allocated(process->mm, (void*)page,
                                 VMM_USER | VMM_WRITABLE, NULL);
    }
    return -EFAULT;
}
