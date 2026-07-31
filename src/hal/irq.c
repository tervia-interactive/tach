/* tach - Architecture-neutral IRQ registry and interrupt state helpers. */
#include <kernel/errno.h>
#include <kernel/string.h>
#include <hal/irq.h>

#define HAL_IRQ_COUNT 256

struct irq_slot {
    irq_handler_t handler;
    void* argument;
};

static struct irq_slot g_irq_slots[HAL_IRQ_COUNT];
#ifdef TACH_HOST_TEST
static bool g_host_irq_enabled;
#endif

void hal_irq_enable(void) {
#ifdef TACH_HOST_TEST
    g_host_irq_enabled = true;
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("sti" ::: "memory");
#elif defined(__aarch64__)
    __asm__ volatile("msr daifclr, #2" ::: "memory");
#elif defined(__arm__)
    __asm__ volatile("cpsie i" ::: "memory");
#elif defined(__riscv)
    __asm__ volatile("csrsi sstatus, 2" ::: "memory");
#endif
}

void hal_irq_disable(void) {
#ifdef TACH_HOST_TEST
    g_host_irq_enabled = false;
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("cli" ::: "memory");
#elif defined(__aarch64__)
    __asm__ volatile("msr daifset, #2" ::: "memory");
#elif defined(__arm__)
    __asm__ volatile("cpsid i" ::: "memory");
#elif defined(__riscv)
    __asm__ volatile("csrci sstatus, 2" ::: "memory");
#endif
}

bool hal_irq_is_enabled(void) {
#ifdef TACH_HOST_TEST
    return g_host_irq_enabled;
#elif defined(__x86_64__) || defined(__i386__)
    uintptr_t flags;
    __asm__ volatile("pushf; pop %0" : "=r"(flags));
    return (flags & (1u << 9)) != 0;
#elif defined(__aarch64__)
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    return (daif & (1u << 7)) == 0;
#elif defined(__arm__)
    uintptr_t cpsr;
    __asm__ volatile("mrs %0, cpsr" : "=r"(cpsr));
    return (cpsr & (1u << 7)) == 0;
#elif defined(__riscv)
    uintptr_t status;
    __asm__ volatile("csrr %0, sstatus" : "=r"(status));
    return (status & 2u) != 0;
#else
    return false;
#endif
}

irq_flags_t hal_irq_save(void) {
#ifdef TACH_HOST_TEST
    irq_flags_t flags = g_host_irq_enabled;
    g_host_irq_enabled = false;
    return flags;
#elif defined(__x86_64__) || defined(__i386__)
    irq_flags_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
#elif defined(__aarch64__)
    irq_flags_t flags;
    __asm__ volatile("mrs %0, daif; msr daifset, #2" : "=r"(flags) :: "memory");
    return flags;
#elif defined(__arm__)
    irq_flags_t flags;
    __asm__ volatile("mrs %0, cpsr; cpsid i" : "=r"(flags) :: "memory");
    return flags;
#elif defined(__riscv)
    irq_flags_t flags;
    __asm__ volatile("csrrci %0, sstatus, 2" : "=r"(flags) :: "memory");
    return flags;
#else
    return 0;
#endif
}

void hal_irq_restore(irq_flags_t flags) {
#ifdef TACH_HOST_TEST
    g_host_irq_enabled = flags != 0;
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
#elif defined(__aarch64__)
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
#elif defined(__arm__)
    __asm__ volatile("msr cpsr_c, %0" :: "r"(flags) : "memory");
#elif defined(__riscv)
    __asm__ volatile("csrw sstatus, %0" :: "r"(flags) : "memory");
#else
    (void)flags;
#endif
}

int hal_irq_register(int irq, irq_handler_t handler, void* argument) {
    if (irq < 0 || irq >= HAL_IRQ_COUNT || !handler) return -EINVAL;
    irq_flags_t flags = hal_irq_save();
    if (g_irq_slots[irq].handler) {
        hal_irq_restore(flags);
        return -EBUSY;
    }
    g_irq_slots[irq].handler = handler;
    g_irq_slots[irq].argument = argument;
    hal_irq_restore(flags);
    return 0;
}

int hal_irq_unregister(int irq) {
    if (irq < 0 || irq >= HAL_IRQ_COUNT) return -EINVAL;
    irq_flags_t flags = hal_irq_save();
    memset(&g_irq_slots[irq], 0, sizeof(g_irq_slots[irq]));
    hal_irq_restore(flags);
    return 0;
}

void hal_irq_dispatch(int irq) {
    if (irq < 0 || irq >= HAL_IRQ_COUNT) return;
    irq_handler_t handler = g_irq_slots[irq].handler;
    if (handler) handler(g_irq_slots[irq].argument);
}
