#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/paging.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/process.h>

#define CHECK(expression) do { if (!(expression)) return __LINE__; } while (0)
#define MOCK_FRAMES 64

static uint8_t g_frames[MOCK_FRAMES][PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));
static bool g_frame_used[MOCK_FRAMES];
static uintptr_t g_dummy_root;
static struct process g_process;

phys_addr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < MOCK_FRAMES; i++) {
        if (g_frame_used[i]) continue;
        g_frame_used[i] = true;
        return (phys_addr_t)(uintptr_t)g_frames[i];
    }
    return 0;
}

void pmm_free_frame(phys_addr_t frame) {
    for (size_t i = 0; i < MOCK_FRAMES; i++)
        if (frame == (phys_addr_t)(uintptr_t)g_frames[i])
            g_frame_used[i] = false;
}

int arch_paging_init(void) { return 0; }
void* arch_paging_kernel_root(void) { return &g_dummy_root; }
void* arch_paging_create_root(void) {
    phys_addr_t frame = pmm_alloc_frame();
    return (void*)(uintptr_t)frame;
}
void arch_paging_destroy_root(void* root) {
    pmm_free_frame((phys_addr_t)(uintptr_t)root);
}
void arch_paging_switch(void* root) { (void)root; }
int arch_paging_map(void* root, uintptr_t virtual_address,
                    phys_addr_t physical_address, uint32_t flags) {
    (void)root; (void)virtual_address; (void)physical_address; (void)flags;
    return 0;
}
int arch_paging_unmap(void* root, uintptr_t virtual_address) {
    (void)root; (void)virtual_address;
    return 0;
}

struct process* process_get_current(void) { return &g_process; }

int main(void) {
    memset(g_frame_used, 0, sizeof(g_frame_used));
    memset(&g_process, 0, sizeof(g_process));
    vmm_init();
    struct vmm_context* context = vmm_create_context();
    CHECK(context != NULL);
    g_process.mm = context;
    g_process.user_mode = true;
    g_process.user_stack_top = PROCESS_USER_STACK_TOP;
    g_process.user_stack_bottom = PROCESS_USER_STACK_TOP - PAGE_SIZE;
    g_process.brk_start = 0x20000000u;
    g_process.brk_end = g_process.brk_start + 3 * PAGE_SIZE;

    uintptr_t stack_page = g_process.user_stack_bottom - PAGE_SIZE;
    CHECK(vmm_handle_fault(stack_page + 32, 1) == 0);
    CHECK(g_process.user_stack_bottom == stack_page);
    CHECK(vmm_resolve(context, (void*)stack_page) != 0);
    CHECK(vmm_handle_fault(stack_page - 2 * PAGE_SIZE, 1) == -EFAULT);

    uintptr_t heap_page = g_process.brk_start + PAGE_SIZE;
    CHECK(vmm_handle_fault(heap_page + 17, 1) == 0);
    const char message[] = "tach-vmm";
    CHECK(vmm_copy_to_user(context, (void*)(heap_page + 17), message,
                           sizeof(message)) == 0);
    char copy[sizeof(message)];
    CHECK(vmm_copy_from_user(copy, context, (void*)(heap_page + 17),
                             sizeof(copy)) == 0);
    CHECK(memcmp(copy, message, sizeof(message)) == 0);

    uintptr_t read_only = 0x30000000u;
    CHECK(vmm_map_allocated(context, (void*)read_only, VMM_USER, NULL) == 0);
    CHECK(vmm_handle_fault(read_only, 1) == -EACCES);

    struct vmm_context* clone = vmm_clone_context(context);
    CHECK(clone != NULL);
    phys_addr_t original = vmm_resolve(context, (void*)(heap_page + 17));
    phys_addr_t copied = vmm_resolve(clone, (void*)(heap_page + 17));
    CHECK(original != copied);
    CHECK(memcmp((void*)(uintptr_t)original, (void*)(uintptr_t)copied,
                 sizeof(message)) == 0);
    vmm_destroy_context(clone);
    vmm_destroy_context(context);
    return 0;
}
