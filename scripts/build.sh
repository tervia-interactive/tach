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

if [ -f "build/${ARCH}/bin/tach.iso" ]; then
    echo "✓ ISO created: build/${ARCH}/bin/tach.iso"
elif [ -f "build/${ARCH}/bin/tach.bin" ]; then
    echo "✓ Kernel created: build/${ARCH}/bin/tach.bin"
else
    echo "✗ Build failed"
    exit 1
fi

echo "=== Build complete ==="
