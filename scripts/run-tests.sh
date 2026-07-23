#!/bin/bash
# tach - Test Runner
# Builds with TACH_TEST=1 and runs ktest + userland tests

set -e

ARCH="${1:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== Running tests for ${ARCH} ==="

# Build with tests enabled
cd "$ROOT_DIR"
make TACH_TEST=1 clean
make TACH_TEST=1 ARCH="$ARCH"

echo "Tests compiled successfully."
echo "Note: Full test execution requires QEMU and test harness."
echo "Run manually: ./scripts/run-qemu.sh $ARCH"
