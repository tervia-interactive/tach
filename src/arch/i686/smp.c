#include <kernel/types.h>

void smp_init(void) {
}

int smp_get_cpu_count(void) {
    return 1;
}

int smp_get_current_cpu(void) {
    return 0;
}
