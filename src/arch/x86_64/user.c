#include <proc/process.h>

extern void x86_64_enter_user(struct user_context* context)
    __attribute__((noreturn));

void arch_enter_user(struct user_context* context) {
    x86_64_enter_user(context);
}
