#!/bin/bash
# tach Operating System - QEMU Runner
# Boots the built image in QEMU per architecture

set -e

ARCH="${1:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
KERNEL_BIN="$ROOT_DIR/src/build/${ARCH}/tach.bin"
KERNEL_ISO="$ROOT_DIR/src/build/${ARCH}/tach.iso"

case "$ARCH" in
    i686|i386)
        QEMU="qemu-system-i386"
        if [ -f "$KERNEL_ISO" ]; then
            exec $QEMU -cdrom "$KERNEL_ISO" -boot d
        elif [ -f "$KERNEL_BIN" ]; then
            exec $QEMU -kernel "$KERNEL_BIN"
        fi
        ;;
    x86_64)
        QEMU="qemu-system-x86_64"
        if [ -f "$KERNEL_ISO" ]; then
            exec $QEMU -cdrom "$KERNEL_ISO" -boot d
        elif [ -f "$KERNEL_BIN" ]; then
            exec $QEMU -kernel "$KERNEL_BIN"
        fi
        ;;
    arm32|arm)
        QEMU="qemu-system-arm"
        MACHINE="virt"
        CPU="cortex-a9"
        if [ -f "$KERNEL_BIN" ]; then
            exec $QEMU -M $MACHINE -cpu $CPU -kernel "$KERNEL_BIN" -nographic
        fi
        ;;
    aarch64|arm64)
        QEMU="qemu-system-aarch64"
        MACHINE="virt"
        CPU="cortex-a57"
        if [ -f "$KERNEL_BIN" ]; then
            exec $QEMU -M $MACHINE -cpu $CPU -kernel "$KERNEL_BIN" -nographic
        fi
        ;;
    riscv32)
        QEMU="qemu-system-riscv32"
        MACHINE="virt"
        if [ -f "$KERNEL_BIN" ]; then
            exec $QEMU -M $MACHINE -kernel "$KERNEL_BIN" -nographic
        fi
        ;;
    riscv64)
        QEMU="qemu-system-riscv64"
        MACHINE="virt"
        if [ -f "$KERNEL_BIN" ]; then
            exec $QEMU -M $MACHINE -kernel "$KERNEL_BIN" -nographic
        fi
        ;;
    *)
        echo "Unknown architecture: $ARCH"
        exit 1
        ;;
esac

echo "No kernel found for $ARCH. Build first with: make ARCH=$ARCH"
exit 1
