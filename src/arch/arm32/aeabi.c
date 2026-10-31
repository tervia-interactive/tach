/* tach - ARM EABI runtime hooks
 *
 * The kernel is linked with -nostdlib and, on arm32, without libgcc (the
 * hard-float libgcc of the cross toolchain cannot be linked with this
 * soft-float kernel), so the integer division helpers are implemented below
 * with plain shift/subtract loops. When the
 * divisor is zero they call __aeabi_idiv0/__aeabi_ldiv0, whose libgcc
 * default implementation calls raise(), which does not exist in the kernel.
 * Defining them here keeps libgcc's version (and the raise reference) out of
 * the link. Like most freestanding kernels we return 0 instead of trapping:
 * the argument is the value libgcc would return for the quotient. */

int __aeabi_idiv0(int value) {
    (void)value;
    return 0;
}

long long __aeabi_ldiv0(long long value) {
    (void)value;
    return 0;
}

/* ---- Integer division helpers (no libgcc) ------------------------------
 * These loops use only shifts and compares: any '/' or '%' here would make
 * the compiler call the very helpers being defined. */

unsigned int __aeabi_uidiv(unsigned int n, unsigned int d) {
    if (d == 0) {
        return (unsigned int)__aeabi_idiv0((int)n);
    }
    unsigned int q = 0;
    unsigned int r = 0;
    for (int bit = 31; bit >= 0; bit--) {
        r = (r << 1) | ((n >> bit) & 1u);
        if (r >= d) {
            r -= d;
            q |= 1u << bit;
        }
    }
    return q;
}

int __aeabi_idiv(int n, int d) {
    unsigned int un = n < 0 ? 0u - (unsigned int)n : (unsigned int)n;
    unsigned int ud = d < 0 ? 0u - (unsigned int)d : (unsigned int)d;
    unsigned int q = __aeabi_uidiv(un, ud);
    return ((n < 0) != (d < 0)) ? (int)(0u - q) : (int)q;
}

unsigned long long __udivmod64(unsigned long long n, unsigned long long d,
                               unsigned long long* rem) {
    if (d == 0) {
        *rem = n;
        return (unsigned long long)__aeabi_ldiv0(0);
    }
    unsigned long long q = 0;
    unsigned long long r = 0;
    for (int bit = 63; bit >= 0; bit--) {
        r = (r << 1) | ((n >> bit) & 1ull);
        if (r >= d) {
            r -= d;
            q |= 1ull << bit;
        }
    }
    *rem = r;
    return q;
}

/* Register-pair returns need assembly: {quotient, remainder} come back in
 * r0-r1 / r2-r3, which a C struct return would not give us. */
__asm__(
    ".text\n"
    ".arm\n"
    ".global __aeabi_uidivmod\n"
    ".type __aeabi_uidivmod, %function\n"
    "__aeabi_uidivmod:\n"
    "    push {r0, r1, lr}\n"
    "    bl __aeabi_uidiv\n"
    "    pop {r1, r2, lr}\n"          /* r1 = numerator, r2 = divisor */
    "    mul r3, r0, r2\n"
    "    sub r1, r1, r3\n"            /* r1 = remainder */
    "    bx lr\n"
    ".global __aeabi_idivmod\n"
    ".type __aeabi_idivmod, %function\n"
    "__aeabi_idivmod:\n"
    "    push {r0, r1, lr}\n"
    "    bl __aeabi_idiv\n"
    "    pop {r1, r2, lr}\n"
    "    mul r3, r0, r2\n"
    "    sub r1, r1, r3\n"
    "    bx lr\n"
    ".global __aeabi_uldivmod\n"
    ".type __aeabi_uldivmod, %function\n"
    "__aeabi_uldivmod:\n"
    "    push {r4, lr}\n"
    "    sub sp, sp, #16\n"
    "    add ip, sp, #8\n"
    "    str ip, [sp]\n"              /* 5th word: pointer to remainder slot */
    "    bl __udivmod64\n"
    "    ldr r2, [sp, #8]\n"
    "    ldr r3, [sp, #12]\n"
    "    add sp, sp, #16\n"
    "    pop {r4, pc}\n"
);
