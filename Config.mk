# tach Operating System - Global Configuration
# This file contains global build variables

# Default architecture (can be overridden via make ARCH=x)
ARCH ?= x86_64

# Supported architectures
SUPPORTED_ARCHS := i686 x86_64 arm32 aarch64 riscv32 riscv64

# Target triplet based on architecture
ifeq ($(ARCH),i686)
    TARGET := i686-elf
    CROSS_COMPILE := i686-elf-
    LINKER_SCRIPT := link/i686.ld
    CFLAGS_ARCH := -m32 -march=i686
    ASMFLAGS_ARCH := --32
endif

ifeq ($(ARCH),x86_64)
    TARGET := x86_64-elf
    CROSS_COMPILE := x86_64-elf-
    LINKER_SCRIPT := link/x86_64.ld
    CFLAGS_ARCH := -m64 -mno-red-zone -mcmodel=large
    ASMFLAGS_ARCH := --64
endif

ifeq ($(ARCH),arm32)
    TARGET := arm-none-eabi
    CROSS_COMPILE := arm-none-eabi-
    LINKER_SCRIPT := link/arm32.ld
    CFLAGS_ARCH := -marm -march=armv7-a
    ASMFLAGS_ARCH := 
endif

ifeq ($(ARCH),aarch64)
    TARGET := aarch64-none-elf
    CROSS_COMPILE := aarch64-none-elf-
    LINKER_SCRIPT := link/aarch64.ld
    CFLAGS_ARCH := -mstrict-align
    ASMFLAGS_ARCH := 
endif

ifeq ($(ARCH),riscv32)
    TARGET := riscv32-unknown-elf
    CROSS_COMPILE := riscv64-linux-gnu-
    LINKER_SCRIPT := link/riscv32.ld
    CFLAGS_ARCH := -march=rv32imac -mabi=ilp32
    ASMFLAGS_ARCH := 
endif

ifeq ($(ARCH),riscv64)
    TARGET := riscv64-unknown-elf
    CROSS_COMPILE := riscv64-linux-gnu-
    LINKER_SCRIPT := link/riscv64.ld
    CFLAGS_ARCH := -march=rv64imac -mabi=lp64
    ASMFLAGS_ARCH := 
endif

# Compiler flags
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

# Common C flags
CFLAGS_COMMON := -ffreestanding -fno-stack-protector -fno-builtin \
                 -Wall -Wextra -Werror -Wpedantic \
                 -nostdlib -nodefaultlibs \
                 -I$(CURDIR)/include

# Debug vs Release
ifdef DEBUG
    CFLAGS_DEBUG := -g -O0 -DTACH_DEBUG=1
else
    CFLAGS_DEBUG := -O2 -DNDEBUG
endif

# Test mode
ifdef TACH_TEST
    CFLAGS_TEST := -DTACH_TEST=1
else
    CFLAGS_TEST := 
endif

# Combined C flags
CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_ARCH) $(CFLAGS_DEBUG) $(CFLAGS_TEST)

# Assembly flags
ASMFLAGS := $(ASMFLAGS_ARCH)

# Build directories
BUILD_DIR := build/$(ARCH)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
ISO_DIR := $(BUILD_DIR)/iso

# Output files
KERNEL_BIN := $(BIN_DIR)/tach.bin
KERNEL_ELF := $(BIN_DIR)/tach.elf
KERNEL_ISO := $(BIN_DIR)/tach.iso

# Version info
TACH_VERSION_MAJOR := 0
TACH_VERSION_MINOR := 1
TACH_VERSION_PATCH := 0
TACH_VERSION_DEV := 1
TACH_VERSION_STR := "$(TACH_VERSION_MAJOR).$(TACH_VERSION_MINOR).$(TACH_VERSION_PATCH)"
ifdef TACH_VERSION_DEV
    TACH_VERSION_STR := $(TACH_VERSION_STR)-dev
endif

# Export variables for sub-makes
export ARCH TARGET CROSS_COMPILE CC AS LD OBJCOPY OBJDUMP
export CFLAGS ASMFLAGS BUILD_DIR OBJ_DIR BIN_DIR ISO_DIR
export KERNEL_BIN KERNEL_ELF KERNEL_ISO
export TACH_VERSION_STR
