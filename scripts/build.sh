#!/bin/bash
# tach Operating System - Build Script
# One-shot build for a given ARCH

set -e

ARCH="${1:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== Building tach for ${ARCH} ==="

cd "$ROOT_DIR"
make clean 2>/dev/null || true
make ARCH="$ARCH"

# i686 and x86_64 need a GRUB ISO to boot correctly under QEMU:
# QEMU's built-in -kernel multiboot loader only accepts 32-bit ELF, so
# x86_64 must be booted via GRUB+ISO (see run-qemu.sh). Build it here
# by default so run-qemu.sh doesn't silently fall back to -kernel.
case "$ARCH" in
    i686|x86_64)
        make iso ARCH="$ARCH"
        ;;
esac

if [ -f "src/build/${ARCH}/tach.iso" ]; then
    echo "✓ ISO created: src/build/${ARCH}/tach.iso"
elif [ -f "src/build/${ARCH}/tach.bin" ]; then
    echo "✓ Kernel created: src/build/${ARCH}/tach.bin"
else
    echo "✗ Build failed"
    exit 1
fi

echo "=== Build complete ==="
