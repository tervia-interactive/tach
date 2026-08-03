#include <proc/process.h>
extern void aarch64_enter_user(struct user_context*) __attribute__((noreturn));
bool arch_user_supported(void) { return true; }
void arch_set_kernel_stack(uintptr_t stack_top) { (void)stack_top; }
void arch_enter_user(struct user_context* context) { aarch64_enter_user(context); }
