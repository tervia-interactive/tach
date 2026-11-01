/* tach - Version Header */
/* Kernel build/version info (backs `uname`) */

#ifndef _KERNEL_VERSION_H
#define _KERNEL_VERSION_H

#include "kernel/types.h"

/* Version components */
#define TACH_VERSION_MAJOR  0
#define TACH_VERSION_MINOR  0
#define TACH_VERSION_PATCH  12
#define TACH_VERSION_DEV    0

/* Version string */
#define TACH_VERSION_STRING "0.0.12"
#define TACH_NAME           "tach"
#define TACH_RELEASE        "tach-0.0.12"

/* Build info */
extern const char* tach_build_date;
extern const char* tach_build_time;
extern const char* tach_compiler;
extern const char* tach_arch;

/* Get version info */
const char* tach_get_version_string(void);
int tach_get_version_major(void);
int tach_get_version_minor(void);
int tach_get_version_patch(void);

/* Uname structure */
typedef struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
} utsname_t;

/* Get system info */
int uname(utsname_t* buf);

#endif /* _KERNEL_VERSION_H */
