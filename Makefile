# tach Root Makefile

include Config.mk

HOST_CC ?= cc

.PHONY: all clean test test-host i686 x86_64 arm32 aarch64 riscv32 riscv64 iso

all: $(ARCH)

i686 x86_64 arm32 aarch64 riscv32 riscv64:
	@echo "Building tach for $@..."
	$(MAKE) -C src ARCH=$@ CROSS_COMPILE=$(CROSS_COMPILE)

iso:
	@echo "Creating bootable ISO for $(ARCH)..."
	$(MAKE) -C src iso ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

test: test-host

test-host:
	@mkdir -p src/build/host
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror -fno-builtin -Iinclude \
		tests/host/userspace_test.c \
		src/lib/string.c src/lib/printf.c src/kernel/klog.c \
		src/fs/vfs.c src/proc/fd.c src/proc/process.c \
		src/proc/scheduler.c src/proc/syscall.c \
		src/term/tty.c src/term/vterm.c src/term/shell.c \
		src/userland/runtime.c \
		-o src/build/host/userspace-test
	@src/build/host/userspace-test
	@echo "Host userspace integration test passed."

clean:
	rm -rf src/build/
	find src -name "*.o" -delete
	find src -name "*.bin" -delete
