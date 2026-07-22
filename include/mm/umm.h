/* tach Operating System - User Memory Manager Header */
#ifndef _MM_UMM_H
#define _MM_UMM_H

#include <kernel/types.h>
#include <stdint.h>

/* Initialize user memory manager */
void umm_init(void);

/* User space brk syscall implementation */
void* umm_brk(void* addr);

/* User space mmap syscall implementation */
void* umm_mmap(void* addr, size_t length, int prot, int flags, int fd, int64_t offset);

/* User space munmap syscall implementation */
int umm_munmap(void* addr, size_t length);

#endif /* _MM_UMM_H */
