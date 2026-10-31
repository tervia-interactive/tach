/* tach - rv32 64-bit division helpers
 *
 * The cross toolchain's libgcc is built for rv64 and cannot be linked into
 * the rv32 kernel, so the 64-bit helpers GCC emits calls to are provided
 * here. Shift/subtract only: a '/' or '%' on 64-bit values would recurse. */

typedef unsigned long long u64;
typedef long long s64;

static u64 divmod_u64(u64 n, u64 d, u64* rem) {
    if (d == 0) {
        *rem = n;
        return 0;
    }
    u64 q = 0;
    u64 r = 0;
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

u64 __udivdi3(u64 n, u64 d) {
    u64 rem;
    return divmod_u64(n, d, &rem);
}

u64 __umoddi3(u64 n, u64 d) {
    u64 rem;
    divmod_u64(n, d, &rem);
    return rem;
}

s64 __divdi3(s64 n, s64 d) {
    u64 un = n < 0 ? 0ull - (u64)n : (u64)n;
    u64 ud = d < 0 ? 0ull - (u64)d : (u64)d;
    u64 rem;
    u64 q = divmod_u64(un, ud, &rem);
    return ((n < 0) != (d < 0)) ? (s64)(0ull - q) : (s64)q;
}

s64 __moddi3(s64 n, s64 d) {
    u64 un = n < 0 ? 0ull - (u64)n : (u64)n;
    u64 ud = d < 0 ? 0ull - (u64)d : (u64)d;
    u64 rem;
    divmod_u64(un, ud, &rem);
    return n < 0 ? (s64)(0ull - rem) : (s64)rem;
}
