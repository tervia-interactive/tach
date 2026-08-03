/* Default privilege-transition hooks for architectures not wired yet. */
#include <proc/process.h>

__attribute__((weak)) void arch_user_init(void) {}
__attribute__((weak)) bool arch_user_supported(void) { return false; }
__attribute__((weak)) void arch_set_kernel_stack(uintptr_t stack_top) {
    (void)stack_top;
}
__attribute__((weak, noreturn)) void arch_enter_user(struct user_context* context) {
    if (context && context->pc) {
        void (*entry)(void) = (void (*)(void))context->pc;
        entry();
    }
    process_exit(127);
    for (;;) {}
}
