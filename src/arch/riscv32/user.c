#include <proc/process.h>
extern void riscv32_enter_user(struct user_context*) __attribute__((noreturn));
bool arch_user_supported(void) { return true; }
void arch_set_kernel_stack(uintptr_t stack_top) {
    __asm__ volatile("csrw sscratch, %0" :: "r"(stack_top) : "memory");
}
void arch_enter_user(struct user_context* context) { riscv32_enter_user(context); }
