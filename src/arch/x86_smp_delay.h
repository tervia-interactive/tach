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

/* Busy-wait roughly `us` microseconds using PIT channel 2 (one-shot). */
static inline void x86_delay_us(uint32_t us) {
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
        for (uint32_t guard = 0; guard < 50000000u; guard++) {
            if (x86d_inb(0x61) & 0x20u) break;
            __asm__ volatile("pause");
        }
        x86d_outb(0x61, (uint8_t)(gate & 0xfcu));
    }
}

#endif /* _ARCH_X86_SMP_DELAY_H */
