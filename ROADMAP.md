# tach roadmap (from 0.0.10)

Direction: **freedom, practicality, stability and universality.** The system is
meant to become large, but every large part is optional. The user decides how
tach runs: headless, text console, or graphical with a compositor; on a PC,
an embedded board or an odd device.

## Principles

1. **Core is always small and stable.** Kernel, VFS, scheduler, syscalls and the
   base libc/shell must build and pass `make test` with every optional layer off.
2. **Everything else is a layer you opt into.** No layer may be required by
   another layer's absence: disabling it must still build and boot.
3. **Universal by default.** New code goes through the HAL; arch-specific code
   lives in `src/arch/<arch>` only. A feature may be unavailable on an
   architecture, but must degrade cleanly instead of breaking the build.
4. **Stability over features.** A layer is marked *stable* only once it has host
   tests and boots under QEMU.

## Planned profiles (not implemented yet)

| Profile    | Intent                                              |
|------------|-----------------------------------------------------|
| `core`     | Kernel + initrd + shell. Tiny devices, bring-up     |
| `console`  | + FAT32/block drivers, full `/bin` utilities, TTY   |
| `desktop`  | + framebuffer, input stack, compositor, GUI toolkit |
| `custom`   | Pick individual layers                              |

## Milestones

- [ ] Build-time layer selection (`PROFILE=` / per-layer switches in `Config.mk`)
- [ ] Framebuffer driver abstraction (linear FB from multiboot/device tree)
- [ ] Input stack beyond PS/2 (USB HID, virtio-input)
- [ ] Userspace display server / compositor (optional, `desktop` profile)
- [ ] More storage and bus drivers (virtio-blk, NVMe, USB mass storage)
- [ ] Networking (virtio-net, TCP/IP) as an optional layer
- [ ] Finish userspace isolation on arm32, aarch64, riscv32, riscv64
- [ ] Device-tree driven hardware discovery for non-PC boards

## Scheduling note

ARM and RISC-V work (userspace isolation, drivers, userland) is planned for 0.9. Until then, 0.0.x and 0.1 focus on x86: `/dev` and VFS, shell, build profiles, framebuffer, AHCI and USB HID.
