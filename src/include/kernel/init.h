/* tach - Initialization Header */
/* Ordered initialization macros */

#ifndef _KERNEL_INIT_H
#define _KERNEL_INIT_H

#include "kernel/compiler.h"

/* Initialization levels */
#define INIT_LEVEL_EARLY    0
#define INIT_LEVEL_HAL      1
#define INIT_LEVEL_MM       2
#define INIT_LEVEL_SCHED    3
#define INIT_LEVEL_DRIVERS  4
#define INIT_LEVEL_FS       5
#define INIT_LEVEL_USERLAND 6

/* Initcall macro for ordered initialization */
#define INITCALL(level, func) \
    static void (*_initcall_##func)(void) USED SECTION(".initcall." #level) = func

/* Convenience macros for each level */
#define EARLY_INIT(func)    INITCALL(INIT_LEVEL_EARLY, func)
#define HAL_INIT(func)      INITCALL(INIT_LEVEL_HAL, func)
#define MM_INIT(func)       INITCALL(INIT_LEVEL_MM, func)
#define SCHED_INIT(func)    INITCALL(INIT_LEVEL_SCHED, func)
#define DRIVER_INIT(func)   INITCALL(INIT_LEVEL_DRIVERS, func)
#define FS_INIT(func)       INITCALL(INIT_LEVEL_FS, func)
#define USERLAND_INIT(func) INITCALL(INIT_LEVEL_USERLAND, func)

/* Run all init calls at a given level */
void run_initcalls(int level);

/* Main initialization function */
void kernel_init(void);

#endif /* _KERNEL_INIT_H */
