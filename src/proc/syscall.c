#include <kernel/types.h>
#include <proc/syscall.h>
void syscall_init(void) {}
long syscall_dispatch(int num, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    (void)num;(void)arg1;(void)arg2;(void)arg3;(void)arg4;(void)arg5;(void)arg6;
    return 0;
}
void syscall_register(int num, syscall_handler_t handler) {(void)num;(void)handler;}
