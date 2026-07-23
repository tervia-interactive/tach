# tach Userland Makefile

ARCH ?= x86_64
CURDIR := $(shell pwd)
ROOTDIR := $(CURDIR)/..

include $(ROOTDIR)/Config.mk

# Cross-compiler setup (same as kernel)
ifeq ($(ARCH),i686)
    CROSS_COMPILE :=
    ARCH_CFLAGS := -m32 -mno-mmx -mno-sse
else ifeq ($(ARCH),x86_64)
    CROSS_COMPILE :=
    ARCH_CFLAGS := -m64 -mno-red-zone -mcmodel=large -mgeneral-regs-only
else ifeq ($(ARCH),arm32)
    override CROSS_COMPILE := arm-linux-gnueabihf-
    ARCH_CFLAGS := -march=armv7-a -mfloat-abi=softfp
else ifeq ($(ARCH),aarch64)
    override CROSS_COMPILE := aarch64-linux-gnu-
    ARCH_CFLAGS := -mno-outline-atomics
else ifeq ($(ARCH),riscv32)
    override CROSS_COMPILE := riscv64-linux-gnu-
    ARCH_CFLAGS := -march=rv32imac_zicsr -mabi=ilp32
else ifeq ($(ARCH),riscv64)
    override CROSS_COMPILE := riscv64-linux-gnu-
    ARCH_CFLAGS := -march=rv64imac_zicsr -mabi=lp64
endif

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AR := $(CROSS_COMPILE)ar

INCLUDE_DIR := $(ROOTDIR)/include/userland
BUILD_DIR := $(ROOTDIR)/src/build/$(ARCH)/userland
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

# Library sources
LIB_SRCS := string.c printf.c malloc.c syscall.c ctype.c
LIB_OBJS := $(patsubst %.c,$(LIB_DIR)/%.o,$(LIB_SRCS))
LIB_STATIC := $(LIB_DIR)/libtach.a

# Binary sources
BIN_SRCS := shell.c init.c
BIN_OBJS := $(patsubst %.c,$(BIN_DIR)/%.o,$(BIN_SRCS))
BINS := $(patsubst %.c,$(BIN_DIR)/%,$(BIN_SRCS))

CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin \
          -fno-pic -fno-pie \
          -Wall -Wextra -nostdlib -nodefaultlibs \
          -I$(INCLUDE_DIR) $(ARCH_CFLAGS) -O2 -DNDEBUG

all: $(BUILD_DIR) $(LIB_STATIC) $(BINS)

$(BUILD_DIR):
	mkdir -p $(LIB_DIR)
	mkdir -p $(BIN_DIR)

$(LIB_DIR)/%.o: lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_STATIC): $(LIB_OBJS)
	$(AR) rcs $@ $^

$(BIN_DIR)/%.o: bin/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(BIN_DIR)/%.o $(LIB_STATIC)
	$(LD) -o $@ $< -L$(LIB_DIR) -ltach

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
