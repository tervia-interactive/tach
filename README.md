# tach

A stable, minimal operating system written in C with support for multiple architectures.

## Overview

**tach** is a hobby operating system designed to be:
- **Stable**: Built with solid foundations and proper error handling
- **Minimal**: Only essential components, no bloat
- **Portable**: Supports x86, x86_64, ARM, ARM64, RISC-V 32/64
- **Modern**: Uses modern development practices and CI/CD

## Supported Architectures

| Architecture | Status | Boot Method |
|-------------|--------|-------------|
| i686 (32-bit x86) | ✅ Working | Multiboot (GRUB) + ISO |
| x86_64 (64-bit x86) | ✅ Working | Multiboot (GRUB) + ISO |
| arm32 (32-bit ARM) | 🚧 In Progress | Device Tree |
| aarch64 (64-bit ARM) | 🚧 In Progress | Device Tree |
| riscv32 (32-bit RISC-V) | 🚧 In Progress | SBI |
| riscv64 (64-bit RISC-V) | 🚧 In Progress | SBI |

## Building

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install -y \
    nasm \
    xorriso \
    grub-pc-bin \
    mtools \
    qemu-system-x86 \
    gcc make

# Cross-compilers (see OSDev wiki for installation)
# i686-elf-gcc, x86_64-elf-gcc, arm-none-eabi-gcc, etc.
```

### Build Commands

```bash
# Build for default architecture (x86_64)
make

# Build for specific architecture
make ARCH=i686
make ARCH=x86_64
make ARCH=arm32
make ARCH=aarch64
make ARCH=riscv32
make ARCH=riscv64

# Build all architectures
make all-archs

# Clean build
make clean

# Run tests
make test

# Build crt0, libc, init, shell, and external commands for any target
make userland ARCH=x86_64
make userland ARCH=aarch64

# Build an x86 ISO. The userspace initrd is generated and attached by default.
make iso ARCH=x86_64

# A custom USTAR image may still override the default payload.
make iso ARCH=x86_64 INITRD="$PWD/custom-initrd.tar"

# Format code
make format
```

## Running with QEMU

```bash
# x86/x86_64 (using ISO)
qemu-system-i386 -cdrom src/build/i686/tach.iso
qemu-system-x86_64 -cdrom src/build/x86_64/tach.iso

# Boot with a persistent FAT32 SATA disk (mounted by tach at /disk)
truncate -s 64M tach-disk.img
mkfs.fat -F 32 tach-disk.img
qemu-system-x86_64 -machine q35 \
  -cdrom src/build/x86_64/tach.iso \
  -drive file=tach-disk.img,format=raw,if=ide

# Or using kernel directly
qemu-system-i386 -kernel src/build/i686/tach.bin
qemu-system-x86_64 -kernel src/build/x86_64/tach.bin

# ARM
qemu-system-arm -M virt -cpu cortex-a9 -kernel src/build/arm32/tach.bin -nographic

# ARM64
qemu-system-aarch64 -M virt -cpu cortex-a57 -kernel src/build/aarch64/tach.bin -nographic

# RISC-V 32
qemu-system-riscv32 -M virt -kernel src/build/riscv32/tach.bin -nographic

# RISC-V 64
qemu-system-riscv64 -M virt -kernel src/build/riscv64/tach.bin -nographic
```

## Using Scripts

```bash
# One-shot build
./scripts/build.sh x86_64

# Run in QEMU
./scripts/run-qemu.sh x86_64

# Format code
./scripts/format.sh

# Run tests
./scripts/run-tests.sh x86_64
```

## Project Structure

```
tach/
├── Config.mk              # Global build configuration
├── Makefile               # Root Makefile
├── rules.mk               # Generic build rules
├── README.md              # This file
├── LICENSE                # Apache License 2.0
├── NOTICE                 # Third-party notices
│
├── scripts/               # Build and utility scripts
│   ├── build.sh           # One-shot build script
│   ├── run-qemu.sh        # QEMU runner
│   ├── format.sh          # Code formatter
│   └── run-tests.sh       # Test runner
│
├── link/                  # Linker scripts per architecture
│   ├── i686.ld
│   ├── x86_64.ld
│   ├── arm32.ld
│   ├── aarch64.ld
│   ├── riscv32.ld
│   └── riscv64.ld
│
├── include/               # Public kernel headers
│   ├── kernel/            # Core kernel interfaces
│   ├── hal/               # Hardware Abstraction Layer
│   ├── boot/              # Bootloader structures
│   ├── hw/                # Hardware discovery
│   ├── mm/                # Memory management
│   ├── proc/              # Process management
│   ├── fs/                # Filesystem
│   ├── drivers/           # Device drivers
│   └── term/              # Terminal subsystem
│
├── src/                   # Kernel source code
│   ├── kernel/            # Core kernel logic
│   ├── arch/              # Architecture-specific code
│   │   ├── i686/
│   │   ├── x86_64/
│   │   ├── arm32/
│   │   ├── aarch64/
│   │   ├── riscv32/
│   │   └── riscv64/
│   └── ...                # Other subsystems
│
├── tests/                 # Test suites
│   ├── kernel/            # Kernel-level tests
│   └── userland/          # Userland tests
│
├── user/                  # Freestanding ring-3 programs
│   ├── crt/               # Architecture-specific _start
│   ├── libc/              # Minimal syscall libc and brk allocator
│   ├── init.c             # PID 1 supervisor
│   └── sh.c               # Userspace shell
│
└── .github/workflows/     # GitHub Actions CI/CD
    └── build.yml          # Automated builds
```

## Features

### Kernel Core
- Modular initialization with ordered initcalls
- Kernel panic with register dump
- Assertion macros (KASSERT)
- Atomic operations for SMP safety
- Spinlocks, mutexes, RW locks
- Reference counting
- Per-CPU data support
- Timer wheel for delayed work

### Memory Management
- Bitmap physical frame allocator populated from Multiboot memory maps
- Reference-counted physical frames and copy-on-write `fork`
- Per-process VMM contexts backed by x86, ARM, AArch64, and RISC-V page tables
- Page allocation, fixed mappings, unmapping, address-space switching, and cleanup
- PMM-backed `kmalloc`, `kzalloc`, `krealloc`, and `kfree`
- Fixed-size slab caches layered over the kernel heap
- W^X enforcement

### IPC & Synchronization
- Message ports (XPC-style)
- Counting semaphores
- kqueue event notification
- POSIX-style signal actions, pending delivery, fatal defaults, and `SIGCHLD`

### Userspace Foundation
- Static process table with PID/PPID, lifecycle state, credentials, and accounting
- 100 Hz preemptive round-robin scheduling driven by PIT, ARM generic timer,
  and RISC-V timer interrupts
- Per-process file descriptor tables with inherited console standard streams
- Syscalls for I/O plus `fork`, `execve`, `waitpid`, `kill`, `sigaction`,
  `brk`, `mmap`, `munmap`, `getdents`, `lseek`, `mkdir`, `unlink`, and `sync`
- Read-only shared fork mappings with first-write page-fault separation
- Freestanding `_start`/crt0 and a minimal libc with syscall wrappers,
  string routines, and a `brk`-backed allocator
- External `/sbin/init` as PID 1, using `fork`, `execve`, and `waitpid` to
  supervise the isolated `/bin/sh`
- Separate ELF utilities in `/bin`: `ls`, `cat`, `mkdir`, `rm`, `touch`,
  `write`, `cp`, `sync`, and `uname`
- Embedded init/shell retained only as a no-initrd fallback
- `/dev/console` and `/dev/tty` through the VFS character-device interface
- Interactive EN-US shell with quoting, command status, history, and line editing
- Serial and PS/2 keyboard input with mirrored serial/VGA output
- ELF32/ELF64 executable validation and `PT_LOAD` segment mapping
- USTAR initrd import and external ELF execution from the shell (`/bin` searched)
- x86 and x86_64 ring-3 entry through per-process TSS kernel stacks,
  DPL3 `int 0x80`, and `iret`/`iretq`
- ARM32 User mode, AArch64 EL0, and RISC-V U-mode entry with native
  `svc`/`ecall` syscall and page-fault paths
- Demand-grown user stacks and heap pages; invalid user faults become `SIGSEGV`
- FIFO semaphores that block and wake processes through the scheduler

Every architecture embeds its matching USTAR userspace payload in the kernel.
On x86 targets, `make iso` also attaches it as a boot module. ELF processes run
in ring 3, ARM User/EL0, or RISC-V U-mode and enter the kernel through the
architecture's native syscall instruction.

### Hardware Support
- VGA text mode console
- Serial port (UART) debugging
- PS/2 keyboard
- AHCI 1.0 SATA DMA on PCI x86 systems (command list, received FIS, PRDT)
- Writable FAT32 volumes, including MBR FAT32 partitions, mounted at `/disk`
- Persistent create/read/write/truncate/unlink/mkdir and mirrored FAT updates

The current FAT32 writer uses short 8.3 names. Long-file-name entries are
ignored safely and are planned as a separate extension.
- USB (planned)
- ACPI (x86) / Device Tree (ARM/RISC-V)
- SMP startup through x86 INIT/SIPI, ARM PSCI CPU_ON, and RISC-V SBI HSM
- Per-CPU scheduler queues, current-process state, TSS state, and timer setup

## CI/CD

GitHub Actions automatically:
- Builds for all supported architectures on every push
- Creates bootable ISOs for x86/x86_64
- Uploads artifacts for download
- Creates releases on version tags

## Development Status

This is an **active development** project. Many features are stubs or placeholders.
See individual component headers for implementation status.

## License

Copyright 2026 Tervia Interactive™

Licensed under the Apache License, Version 2.0.
See [LICENSE](LICENSE) for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run `make format` to format code
5. Submit a pull request

## Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [ARM Documentation](https://developer.arm.com/documentation)
- [RISC-V Specs](https://riscv.org/specifications/)
