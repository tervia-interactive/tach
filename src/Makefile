# tach OS - Main Source Makefile
# Builds kernel for specified architecture

include ../Config.mk
include ../rules.mk

ARCH ?= x86_64
BUILD_DIR := ../build/$(ARCH)
KERNEL_BIN := $(BUILD_DIR)/bin/tach.bin

# Architecture-specific settings
ifeq ($(ARCH),i686)
CROSS_COMPILE := i686-elf-
LINKER_SCRIPT := ../link/i686.ld
ARCH_DIR := arch/i686
endif
ifeq ($(ARCH),x86_64)
CROSS_COMPILE := x86_64-elf-
LINKER_SCRIPT := ../link/x86_64.ld
ARCH_DIR := arch/x86_64
endif
ifeq ($(ARCH),arm32)
CROSS_COMPILE := arm-none-eabi-
LINKER_SCRIPT := ../link/arm32.ld
ARCH_DIR := arch/arm32
endif
ifeq ($(ARCH),aarch64)
CROSS_COMPILE := aarch64-none-elf-
LINKER_SCRIPT := ../link/aarch64.ld
ARCH_DIR := arch/aarch64
endif
ifeq ($(ARCH),riscv32)
CROSS_COMPILE := riscv32-elf-
LINKER_SCRIPT := ../link/riscv32.ld
ARCH_DIR := arch/riscv32
endif
ifeq ($(ARCH),riscv64)
CROSS_COMPILE := riscv64-elf-
LINKER_SCRIPT := ../link/riscv64.ld
ARCH_DIR := arch/riscv64
endif

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld

# Collect all source files
C_SOURCES := $(shell find kernel sync ipc hal boot hw mm proc fs drivers term lib -name "*.c" 2>/dev/null)
ASM_SOURCES := $(shell find $(ARCH_DIR) -name "*.S" 2>/dev/null)
ARCH_C_SOURCES := $(shell find $(ARCH_DIR) -name "*.c" 2>/dev/null)

C_OBJECTS := $(C_SOURCES:.c=.o)
ARCH_C_OBJECTS := $(ARCH_C_SOURCES:.c=.o)
ASM_OBJECTS := $(ASM_SOURCES:.S=.o)

OBJECTS := $(C_OBJECTS) $(ARCH_C_OBJECTS) $(ASM_OBJECTS)

all: $(BUILD_DIR) $(KERNEL_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/bin

$(KERNEL_BIN): $(OBJECTS)
	$(LD) -T $(LINKER_SCRIPT) -o $@ $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

iso: $(KERNEL_BIN)
	@echo "Creating ISO for $(ARCH)..."
	mkdir -p $(BUILD_DIR)/iso/boot/grub
	cp $(KERNEL_BIN) $(BUILD_DIR)/iso/boot/kernel.bin
	echo 'set timeout=0' > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'set default=0' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo 'menuentry "tach OS" {' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  multiboot /boot/kernel.bin' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '  boot' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo '}' >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/bin/tach.iso $(BUILD_DIR)/iso 2>/dev/null || echo "Note: grub-mkrescue not available, ISO not created"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all iso clean
