# tach Configuration

# Default architecture
ARCH ?= x86_64

# Cross-compiler prefix (set by CI, empty for native builds)
CROSS_COMPILE ?=

# Target triplet based on architecture
ifeq ($(ARCH),i686)
    TARGET := i686-elf
endif
ifeq ($(ARCH),x86_64)
    TARGET := x86_64-elf
endif
ifeq ($(ARCH),arm32)
    TARGET := arm-linux-gnueabihf
endif
ifeq ($(ARCH),aarch64)
    TARGET := aarch64-linux-gnu
endif
ifeq ($(ARCH),riscv32)
    TARGET := riscv32-linux-gnu
endif
ifeq ($(ARCH),riscv64)
    TARGET := riscv64-linux-gnu
endif

# Compiler and tools
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# Common flags
CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin -fno-pic -fno-pie -Wall -Wextra -nostdlib -nodefaultlibs
CFLAGS += -I$(CURDIR)/include
CFLAGS += -O2 -DNDEBUG

# Architecture-specific flags
ifeq ($(ARCH),i686)
    CFLAGS += -m32 -mno-mmx -mno-sse
    LDFLAGS := -m elf_i386 -T $(CURDIR)/link/i686.ld
endif
ifeq ($(ARCH),x86_64)
    CFLAGS += -m64 -mno-red-zone -mcmodel=large
    LDFLAGS := -m elf_x86_64 -T $(CURDIR)/link/x86_64.ld
endif
ifeq ($(ARCH),arm32)
    CFLAGS += -marm -mcpu=cortex-a7
    LDFLAGS := -m armelf_linux_eabi -T $(CURDIR)/link/arm32.ld
endif
ifeq ($(ARCH),aarch64)
    CFLAGS += -mgeneral-regs-only
    LDFLAGS := -m aarch64elf -T $(CURDIR)/link/aarch64.ld
endif
ifeq ($(ARCH),riscv32)
    CFLAGS += -march=rv32ima_zicsr -mabi=ilp32
    LDFLAGS := -m elf32lriscv -T $(CURDIR)/link/riscv32.ld
endif
ifeq ($(ARCH),riscv64)
    CFLAGS += -march=rv64ima_zicsr -mabi=lp64
    LDFLAGS := -m elf64lriscv -T $(CURDIR)/link/riscv64.ld
endif

# Build directory
BUILD_DIR := $(CURDIR)/build/$(ARCH)

# Output files
KERNEL_BIN := $(BUILD_DIR)/tach.bin
KERNEL_ISO := $(BUILD_DIR)/tach.iso
