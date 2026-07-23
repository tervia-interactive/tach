# tach Source Makefile

ARCH ?= x86_64
CURDIR := $(shell pwd)
ROOTDIR := $(CURDIR)/..

include $(ROOTDIR)/Config.mk

# Config.mk builds LDFLAGS' -T path using $(CURDIR), which here is src/ (not
# the project root), so it points at a nonexistent src/link/*.ld. Rewrite it
# to the real link/ directory at the project root.
LDFLAGS := $(subst $(CURDIR)/link,$(ROOTDIR)/link,$(LDFLAGS))

# Set cross-compiler based on architecture
ifeq ($(ARCH),i686)
    CROSS_COMPILE :=
    ARCH_CFLAGS := -m32 -mno-mmx -mno-sse
    LINKER_SCRIPT := $(ROOTDIR)/link/i686.ld
else ifeq ($(ARCH),x86_64)
    CROSS_COMPILE :=
    ARCH_CFLAGS := -m64 -mno-red-zone -mcmodel=large -mgeneral-regs-only
    LINKER_SCRIPT := $(ROOTDIR)/link/x86_64.ld
else ifeq ($(ARCH),arm32)
    override CROSS_COMPILE := arm-linux-gnueabihf-
    ARCH_CFLAGS := -march=armv7-a -mfloat-abi=softfp
    LINKER_SCRIPT := $(ROOTDIR)/link/arm32.ld
else ifeq ($(ARCH),aarch64)
    override CROSS_COMPILE := aarch64-linux-gnu-
    ARCH_CFLAGS := -mno-outline-atomics
    LINKER_SCRIPT := $(ROOTDIR)/link/aarch64.ld
else ifeq ($(ARCH),riscv32)
    override CROSS_COMPILE := riscv64-linux-gnu-
    ARCH_CFLAGS := -march=rv32imac_zicsr -mabi=ilp32
    LINKER_SCRIPT := $(ROOTDIR)/link/riscv32.ld
else ifeq ($(ARCH),riscv64)
    override CROSS_COMPILE := riscv64-linux-gnu-
    ARCH_CFLAGS := -march=rv64imac_zicsr -mabi=lp64
    LINKER_SCRIPT := $(ROOTDIR)/link/riscv64.ld
endif

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld

INCLUDE_DIR := $(ROOTDIR)/include
SRC_DIRS := kernel sync ipc hal boot hw mm proc fs term lib
SRC_DIRS += drivers/char drivers/video drivers/input drivers/block
SRC_DIRS += arch/$(ARCH)

# Add userland to build
USERLAND_DIR := $(ROOTDIR)/userland

C_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))

# Object files live under build/$(ARCH)/obj, mirroring the source tree,
# rather than next to the .c/.S sources. Building a different ARCH without
# an intervening 'make clean' would otherwise silently reuse object files
# compiled for the previous architecture and fail (or worse, produce a
# broken binary) at link time.
OBJ_DIR := $(BUILD_DIR)/obj
OBJECTS := $(addprefix $(OBJ_DIR)/,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.S=.o))

CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin \
          -fno-pic -fno-pie \
          -Wall -Wextra -nostdlib -nodefaultlibs \
          -I$(INCLUDE_DIR) $(ARCH_CFLAGS) -O2 -DNDEBUG

ifdef TACH_TEST
CFLAGS += -DTACH_TEST
endif

all: $(BUILD_DIR) $(KERNEL_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Building userland..."
	$(MAKE) -C $(USERLAND_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

iso: $(KERNEL_BIN)
ifeq ($(filter $(ARCH),i686 x86_64),)
	@echo "ISO creation only supported for i686 and x86_64"
else
	@command -v grub-mkrescue >/dev/null 2>&1 || { \
		echo "ERROR: grub-mkrescue not found."; \
		echo "Install it with: sudo apt install grub-pc-bin grub-common xorriso mtools"; \
		exit 1; \
	}
	@command -v xorriso >/dev/null 2>&1 || { \
		echo "ERROR: xorriso not found (grub-mkrescue needs it)."; \
		echo "Install it with: sudo apt install grub-pc-bin grub-common xorriso mtools"; \
		exit 1; \
	}
	mkdir -p $(BUILD_DIR)/iso/boot/grub
	cp $(KERNEL_BIN) $(BUILD_DIR)/iso/boot/tach.bin
	cp -r $(USERLAND_DIR)/build/$(ARCH)/bin $(BUILD_DIR)/iso/boot/bin
	echo "set timeout=3" > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "set default=0" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "menuentry \"tach\" {" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "  $(if $(filter $(ARCH),x86_64),multiboot2,multiboot) /boot/tach.bin" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "  boot" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "}" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(BUILD_DIR)/iso
	@test -f $(KERNEL_ISO) || { echo "ERROR: grub-mkrescue ran but $(KERNEL_ISO) was not produced."; exit 1; }
endif

clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete
	find . -name "*.bin" -delete
	$(MAKE) -C $(USERLAND_DIR) clean

.PHONY: all clean iso
