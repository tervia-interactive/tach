# tach - Makefile for ARM architecture
# Copyright 2026 Tervia Interactive™
# Licensed under Apache License 2.0

ARCH = arm
CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = arm-none-eabi-ld
CFLAGS = -ffreestanding -O2 -Wall -Wextra -marm -I$(CURDIR)/kernel/include -DARCH_ARM
LDFLAGS = -T $(CURDIR)/arch/arm/linker.ld

KERNEL_SRCS = $(CURDIR)/kernel/main.c \
              $(CURDIR)/kernel/drivers/vga.c \
              $(CURDIR)/kernel/drivers/serial.c

KERNEL_OBJS = $(KERNEL_SRCS:.c=.o)
BOOT_OBJ = $(CURDIR)/arch/arm/boot.o

KERNEL_BIN = $(CURDIR)/build/$(ARCH)/tach.bin

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJS)
	@mkdir -p $(CURDIR)/build/$(ARCH)
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJ) $(KERNEL_OBJS)

$(CURDIR)/arch/arm/boot.o: $(CURDIR)/arch/arm/boot.s
	$(CC) -c $< -o $@

$(CURDIR)/%.o: $(CURDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(KERNEL_OBJS) $(BOOT_OBJ) $(KERNEL_BIN)

.PHONY: all clean
