/* HAL CPU Dispatcher */
#include <hal/cpu.h>

void hal_cpu_halt(void) {
    /* Architecture-specific halt */
    for (;;) {}
}

void hal_cpu_cli(void) {}
void hal_cpu_sti(void) {}
