/* tach Operating System - Credentials Header */
/* uid/gid/permission struct carried per-process */

#ifndef _KERNEL_CREDENTIALS_H
#define _KERNEL_CREDENTIALS_H

#include "kernel/types.h"

/* Special UID/GID values */
#define UID_ROOT    0
#define GID_ROOT    0
#define UID_NOBODY  65534
#define GID_NOBODY  65534

/* Credential structure */
typedef struct credentials {
    uid_t uid;          /* Real user ID */
    uid_t euid;         /* Effective user ID */
    uid_t suid;         /* Saved set-user-ID */
    gid_t gid;          /* Real group ID */
    gid_t egid;         /* Effective group ID */
    gid_t sgid;         /* Saved set-group-ID */
    uint32_t ngroups;
    gid_t groups[16];   /* Supplementary groups */
} credentials_t;

/* Check if credentials have root privileges */
bool cred_is_root(const credentials_t* cred);

/* Check permission */
bool cred_has_permission(const credentials_t* cred, uid_t owner, gid_t group, uint32_t mode);

/* Copy credentials */
void cred_copy(credentials_t* dst, const credentials_t* src);

/* Initialize credentials for new process */
void cred_init(credentials_t* cred, uid_t uid, gid_t gid);

#endif /* _KERNEL_CREDENTIALS_H */
