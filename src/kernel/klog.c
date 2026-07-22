#include <kernel/types.h>
/* Kernel log ring buffer */
void klog_init(void) {}
void klog_write(const char *msg, size_t len) {(void)msg; (void)len;}
