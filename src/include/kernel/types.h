/* tach - Kernel Types Header */
/* Standardized types for the kernel */

#ifndef _KERNEL_TYPES_H
#define _KERNEL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Null pointer */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Boolean type - only define if not already defined */
#ifndef __bool_true_false_are_defined
typedef unsigned char bool;
#define true  1
#define false 0
#endif

/* Standard integer types (from stdint.h) */
/* int8_t, int16_t, int32_t, int64_t */
/* uint8_t, uint16_t, uint32_t, uint64_t */
/* intptr_t, uintptr_t */

/* Size and pointer difference types */
/* size_t, ptrdiff_t */

/* Kernel-specific types */
typedef uint32_t pid_t;      /* Process ID */
typedef uint32_t uid_t;      /* User ID */
typedef uint32_t gid_t;      /* Group ID */
typedef int32_t  ssize_t;    /* Signed size type */
typedef uint64_t time_t;     /* Time type */
typedef uint64_t tick_t;     /* Timer tick type */
typedef uintptr_t phys_addr_t; /* Physical address type for DMA/MMU */
typedef uintptr_t virt_addr_t; /* Virtual address type */

/* Result type for kernel operations */
typedef int32_t result_t;

/* Success/Failure macros */
#define RESULT_OK       0
#define RESULT_ERROR   -1

#endif /* _KERNEL_TYPES_H */
