/* tach - ARM EABI runtime hooks
 *
 * The kernel is linked with -nostdlib, so libgcc's integer division helpers
 * (__aeabi_uidiv, __aeabi_idivmod, ...) are pulled in explicitly. When the
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
