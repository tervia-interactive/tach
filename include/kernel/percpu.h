/* tach - Per-CPU Data Header */
/* Per-CPU data sections (required for real SMP support) */

#ifndef _KERNEL_PERCPU_H
#define _KERNEL_PERCPU_H

#include "kernel/types.h"
#include "kernel/compiler.h"

/* Per-CPU section attribute */
#define PERCPU SECTION(".percpu")

/* Maximum CPUs supported */
#define MAX_CPUS 256

/* Per-CPU data structure */
typedef struct percpu_data {
    uint32_t cpu_id;
    void* stack_ptr;
    void* current_thread;
    void* current_process;
    uint64_t tick_count;
    uint64_t idle_time;
} percpu_data_t;

/* Get current CPU ID */
uint32_t percpu_get_id(void);

/* Get per-CPU data for current CPU */
percpu_data_t* percpu_get_self(void);

/* Get per-CPU data for specific CPU */
percpu_data_t* percpu_get(uint32_t cpu_id);

/* Initialize per-CPU area */
void percpu_init(void);

#endif /* _KERNEL_PERCPU_H */
