#include <boot/sbi.h>

struct sbi_return sbi_ecall(long eid, long fid, long a0, long a1, long a2,
                            long a3, long a4, long a5) {
#if defined(__riscv)
    register long ra0 __asm__("a0") = a0;
    register long ra1 __asm__("a1") = a1;
    register long ra2 __asm__("a2") = a2;
    register long ra3 __asm__("a3") = a3;
    register long ra4 __asm__("a4") = a4;
    register long ra5 __asm__("a5") = a5;
    register long ra6 __asm__("a6") = fid;
    register long ra7 __asm__("a7") = eid;
    __asm__ volatile("ecall" : "+r"(ra0), "+r"(ra1)
                     : "r"(ra2), "r"(ra3), "r"(ra4), "r"(ra5),
                       "r"(ra6), "r"(ra7) : "memory");
    return (struct sbi_return){.error = ra0, .value = ra1};
#else
    (void)eid; (void)fid; (void)a0; (void)a1; (void)a2;
    (void)a3; (void)a4; (void)a5;
    return (struct sbi_return){.error = -1, .value = 0};
#endif
}

int sbi_call(long eid, long fid, long a0) {
    return (int)sbi_ecall(eid, fid, a0, 0, 0, 0, 0, 0).error;
}
