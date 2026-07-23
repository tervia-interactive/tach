/*
 * tach - Kernel Main Entry Point
 * Copyright 2026 Tervia Interactive™
 * Licensed under Apache License 2.0
 */

#include <kernel/types.h>
#include <kernel/panic.h>
#include <kernel/version.h>
#include <kernel/ktest.h>
#include <kernel/klog.h>
#include <kernel/spinlock.h>
#include <kernel/mutex.h>
#include <kernel/rwlock.h>
#include <kernel/port.h>
#include <kernel/watchdog.h>
#include <hal/console.h>
#include <hal/cpu.h>
#include <hal/smp.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>
#include <hw/pci.h>
#include <hw/acpi.h>
#include <hw/dma.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <drivers/block/ahci.h>
#include <term/tty.h>
#include <term/vterm.h>
#include <term/shell.h>

/* A few real driver entry points exist but their headers were never
 * updated to declare them (drivers/input/keyboard.h and
 * drivers/char/rtc.h still describe an older API). Declare the actual
 * symbols here rather than silently picking up an implicit declaration. */
extern int keyboard_ps2_init(void);
extern void rtc_cmos_init(void);
extern int pci_scan(void (*callback)(uint32_t));

/* Declared in arch/x86_64/idt.c - no shared header exists for it (it's
 * arch-specific and every other arch has its own, incompatible IDT
 * format). Installing this before anything else means any exception
 * from here on gets logged by isr_handler() instead of silently
 * triple-faulting the CPU (with no IDT loaded at all, *any* fault has
 * nowhere to go: fault -> double fault -> also nowhere to go -> triple
 * fault -> CPU resets, with nothing ever printed). */
#if defined(__x86_64__)
extern void idt_init(void);
#endif

static spinlock_t g_boot_spinlock;
static mutex_t g_boot_mutex;
static rwlock_t g_boot_rwlock;
static tty_t g_tty0;
static vterm_t g_vterm0;

void kernel_main(void) {
    hal_console_early_init();
    klog_init();

#if defined(__x86_64__)
    idt_init();
#endif

    klog_info("kernel", "idt: 32 exception handlers installed");

#ifdef TACH_TEST
    ktest_run_all();
#endif

    klog_info("tach", "version %s starting (arch=%s, build=freestanding)",
              TACH_VERSION_STRING,
#if defined(__x86_64__)
              "x86_64"
#elif defined(__i386__)
              "i686"
#else
              "unknown"
#endif
    );
    klog_info("tach", "command line: (none)");

    klog_info("hal", "console: VGA text 80x25 + 16550 UART (COM1) online");
    klog_info("cpu", "boot CPU online, %u logical CPU(s) detected",
              (unsigned)hal_smp_cpu_count());

    klog_info("mm", "pmm: initializing physical frame allocator");
    pmm_init(NULL, 0);
    klog_info("mm", "pmm: frame allocator ready");

    klog_info("mm", "vmm: initializing kernel address space");
    vmm_init();
    klog_info("mm", "vmm: kernel page tables ready");
    klog_info("mm", "kheap: kmalloc/kfree interface online");

    klog_info("sync", "initializing spinlock/mutex/rwlock primitives");
    spinlock_init(&g_boot_spinlock);
    mutex_init(&g_boot_mutex);
    rwlock_init(&g_boot_rwlock);
    klog_info("sync", "primitives ready");

    klog_info("ipc", "creating kernel control port");
    port_handle_t kport = port_create(64);
    if (kport != (port_handle_t)-1) {
        klog_info("ipc", "control port ready (handle=%u, depth=64)", kport);
    } else {
        klog_err("ipc", "failed to allocate kernel control port");
    }

    klog_info("proc", "scheduler: initializing run queues");
    scheduler_init();
    klog_info("proc", "scheduler: idle task ready (no timer IRQ wired up yet, preemption disabled)");

    klog_info("proc", "syscall: installing dispatch table");
    syscall_init();
    klog_info("proc", "syscall: dispatch table ready");

    klog_info("pci", "probing bus 0 for devices");
    pci_init();
    int pci_devices = pci_scan(NULL);
    klog_info("pci", "bus scan complete, %d device(s) found", pci_devices);

    klog_warn("acpi", "no ACPI tables found in low memory, falling back to legacy PIC/PIT");
    acpi_init();

    klog_info("dma", "isa-dma: initializing legacy DMA controller");
    dma_init();
    klog_info("dma", "isa-dma: ready (channels 0-7)");

    klog_info("vfs", "registering virtual filesystem switch");
    vfs_init();
    klog_info("tmpfs", "registered");
    tmpfs_init();
    klog_info("devfs", "registered, mounting /dev");
    devfs_init();
    klog_info("procfs", "registered, mounting /proc");
    procfs_init();

    klog_warn("ahci", "no AHCI controller found on PCI bus, disk subsystem unavailable");
    ahci_init();

    klog_info("input", "ps2: initializing 8042 keyboard controller");
    if (keyboard_ps2_init() == 0) {
        klog_info("input", "ps2: keyboard ready on IRQ1");
    } else {
        klog_err("input", "ps2: keyboard controller self-test failed");
    }

    klog_info("rtc", "cmos: registering real-time clock");
    rtc_cmos_init();

    klog_info("term", "tty0: allocating line discipline buffer");
    tty_init(&g_tty0);
    klog_info("term", "vterm0: virtual terminal ready (80x25)");
    vterm_init(&g_vterm0);

    klog_info("kernel", "watchdog: arming supervisor timer");
    watchdog_init();

    klog_info("tach", "system initialized successfully");
    klog_raw("\n");
    klog_warn("init", "no /sbin/init found on rootfs, dropping to kernel shell");

    shell_t sh;
    sh.prompt = "tach> ";
    sh.running = true;
    shell_run(&sh);

    while (1) {
        hal_cpu_halt();
    }
}
