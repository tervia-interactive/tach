#!/bin/bash
# tach Operating System - Code Formatter
# Runs clang-format against the whole tree

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

if ! command -v clang-format &> /dev/null; then
    echo "clang-format not found. Install it first."
    exit 1
fi

echo "Formatting C/C++ files..."

find "$ROOT_DIR/src" "$ROOT_DIR/include" "$ROOT_DIR/tests" \
     -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) \
     -exec clang-format -i {} \;

echo "Format complete."
