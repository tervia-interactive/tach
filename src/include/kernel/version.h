/* tach - Version Header */
/* Kernel build/version info (backs `uname`) */

#ifndef _KERNEL_VERSION_H
#define _KERNEL_VERSION_H

#include "kernel/types.h"

/* Version components */
#define TACH_VERSION_MAJOR  0
#define TACH_VERSION_MINOR  1
#define TACH_VERSION_PATCH  0
#define TACH_VERSION_DEV    1

/* Version string */
#define TACH_VERSION_STRING "0.1.0-dev"
#define TACH_NAME           "tach"
#define TACH_RELEASE        "tach-0.1.0-dev"

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
