/* tach - x86 SMP bring-up helpers shared by i686 and x86_64.
 *
 * Why this exists: hal_smp_init() runs with interrupts still disabled on the
 * BSP, so the tick counter cannot be used as a clock, and counting `pause`
 * iterations is not real time (under a hypervisor with pause-loop exiting a
 * single `pause` can cost microseconds, turning a "short" loop into minutes).
 * PIT channel 2 is free, polled, and emulated by QEMU/KVM/VirtualBox/VMware.
 */
#ifndef _ARCH_X86_SMP_DELAY_H
#define _ARCH_X86_SMP_DELAY_H

#include <kernel/types.h>

static inline void x86d_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}
static inline uint8_t x86d_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint64_t x86d_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* Set when PIT channel 2 never reported terminal count in a sane time. From
 * then on delays are short spin loops, so a hypervisor with a broken PIT
 * degrades SMP bring-up instead of hanging the boot for hours. */
static int g_x86_pit_delay_broken;

/* Busy-wait roughly `us` microseconds using PIT channel 2 (one-shot). */
static inline void x86_delay_us(uint32_t us) {
    if (g_x86_pit_delay_broken) {
        for (uint32_t i = 0; i < us * 20u; i++) __asm__ volatile("pause");
        return;
    }
    while (us) {
        uint32_t chunk = us > 50000u ? 50000u : us; /* 16-bit counter limit */
        uint32_t count = (uint32_t)(((uint64_t)chunk * 1193182u) / 1000000u);
        if (count < 2u) count = 2u;
        us -= chunk;

        uint8_t gate = x86d_inb(0x61);
        x86d_outb(0x61, (uint8_t)(gate & 0xfcu));       /* gate low, speaker off */
        x86d_outb(0x43, 0xb0);                          /* ch2, lo/hi, mode 0 */
        x86d_outb(0x42, (uint8_t)(count & 0xffu));
        x86d_outb(0x42, (uint8_t)(count >> 8));
        x86d_outb(0x61, (uint8_t)((gate & 0xfcu) | 0x01u)); /* gate high: start */

        /* OUT2 (bit 5) goes high at terminal count. The iteration cap only
         * protects against a platform without PIT ch2; it is never the norm. */
        uint64_t start = x86d_rdtsc();
        for (;;) {
            if (x86d_inb(0x61) & 0x20u) break;
            __asm__ volatile("pause");
            /* ~2^30 cycles (0.3-1 s) is far above the 50 ms maximum chunk. */
            if (x86d_rdtsc() - start > (1ull << 30)) {
                g_x86_pit_delay_broken = 1;
                break;
            }
        }
        x86d_outb(0x61, (uint8_t)(gate & 0xfcu));
    }
}

#endif /* _ARCH_X86_SMP_DELAY_H */
