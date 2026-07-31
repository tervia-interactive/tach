#include <proc/process.h>
extern void i686_enter_user(struct user_context*) __attribute__((noreturn));
void arch_enter_user(struct user_context* context) { i686_enter_user(context); }
