# tach Operating System - Root Makefile
# Main entry point for building the OS
# Copyright 2026 Tervia Interactive™
# Licensed under Apache License 2.0

include Config.mk

# Default target
all: $(ARCH)

# Build targets for each architecture
i686 x86_64 arm32 aarch64 riscv32 riscv64:
	@echo "Building tach for $(ARCH)..."
	$(MAKE) -C src ARCH=$(ARCH)
ifeq ($(ARCH),i686)
	@echo "Creating bootable ISO for i686..."
	$(MAKE) -C src iso ARCH=$(ARCH)
endif
ifeq ($(ARCH),x86_64)
	@echo "Creating bootable ISO for x86_64..."
	$(MAKE) -C src iso ARCH=$(ARCH)
endif
	@echo "Build complete for $(ARCH). Output in $(BUILD_DIR)"

# Legacy architecture names (for backwards compatibility)
i386: i686
arm: arm32
arm64: aarch64

# Build all supported architectures
all-archs:
	@for arch in $(SUPPORTED_ARCHS); do \
	echo "=== Building for $$arch ==="; \
	$(MAKE) ARCH=$$arch; \
	done

# Clean build artifacts
clean:
	rm -rf build/
	@echo "Clean complete."

# Run tests
test:
	@echo "Running kernel tests..."
	$(MAKE) TACH_TEST=1 $(ARCH)
	./scripts/run-tests.sh $(ARCH)

# Format code
format:
	./scripts/format.sh

# Quick run in QEMU
run:
	@echo "Booting tach ($(ARCH)) in QEMU..."
	./scripts/run-qemu.sh $(ARCH)

# Help target
help:
	@echo "tach Operating System Build System"
	@echo ""
	@echo "Usage: make [target] [ARCH=<arch>]"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build for current architecture (default: x86_64)"
	@echo "  i686         Build for 32-bit x86 (creates ISO)"
	@echo "  x86_64       Build for 64-bit x86 (creates ISO)"
	@echo "  arm32        Build for 32-bit ARM"
	@echo "  aarch64      Build for 64-bit ARM"
	@echo "  riscv32      Build for 32-bit RISC-V"
	@echo "  riscv64      Build for 64-bit RISC-V"
	@echo "  all-archs    Build for all supported architectures"
	@echo "  clean        Remove all build artifacts"
	@echo "  test         Build with tests and run test suite"
	@echo "  format       Format code with clang-format"
	@echo "  run          Boot in QEMU for current architecture"
	@echo "  help         Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make x86_64           # Build for x86_64"
	@echo "  make ARCH=i686        # Build for i686"
	@echo "  make DEBUG=1 x86_64   # Build with debug symbols"
	@echo "  make TACH_TEST=1      # Build with tests enabled"

.PHONY: all i686 x86_64 arm32 aarch64 riscv32 riscv64 i386 arm arm64 all-archs clean test format run help
