#include <kernel/types.h>
/* HAL CPU Dispatcher */
#include <hal/hal_cpu.h>

void hal_cpu_halt(void) {
    /* Architecture-specific halt */
    for (;;) {}
}

void hal_cpu_cli(void) {}
void hal_cpu_sti(void) {}
