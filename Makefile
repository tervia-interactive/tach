# tach Root Makefile

include Config.mk

.PHONY: all clean i686 x86_64 arm32 aarch64 riscv32 riscv64 iso

all: $(ARCH)

i686 x86_64 arm32 aarch64 riscv32 riscv64:
	@echo "Building tach for $@..."
	$(MAKE) -C src ARCH=$@ CROSS_COMPILE=$(CROSS_COMPILE)

iso:
	@echo "Creating bootable ISO for $(ARCH)..."
	$(MAKE) -C src iso ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	rm -rf src/build/
	find src -name "*.o" -delete
	find src -name "*.bin" -delete
