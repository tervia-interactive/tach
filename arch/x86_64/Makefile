# tach - Makefile for x86_64 architecture
# Copyright 2026 Tervia Interactive™
# Licensed under Apache License 2.0

ARCH = x86_64
CC = x86_64-elf-gcc
AS = x86_64-elf-as
LD = x86_64-elf-ld
CFLAGS = -ffreestanding -O2 -Wall -Wextra -m64 -I$(CURDIR)/kernel/include -DARCH_X86
LDFLAGS = -m elf_x86_64 -T $(CURDIR)/arch/x86_64/linker.ld

KERNEL_SRCS = $(CURDIR)/kernel/main.c \
              $(CURDIR)/kernel/drivers/vga.c \
              $(CURDIR)/kernel/drivers/serial.c

KERNEL_OBJS = $(KERNEL_SRCS:.c=.o)
BOOT_OBJ = $(CURDIR)/arch/x86_64/boot.o

KERNEL_BIN = $(CURDIR)/build/$(ARCH)/tach.bin
ISO = $(CURDIR)/build/$(ARCH)/tach.iso

all: $(ISO)

$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJS)
	@mkdir -p $(CURDIR)/build/$(ARCH)
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJ) $(KERNEL_OBJS)

$(CURDIR)/arch/x86_64/boot.o: $(CURDIR)/arch/x86_64/boot.asm
	$(CC) -c $< -o $@ -m64

$(CURDIR)/%.o: $(CURDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(ISO): $(KERNEL_BIN)
	@mkdir -p $(CURDIR)/isotmp/$(ARCH)/boot/grub
	cp $(KERNEL_BIN) $(CURDIR)/isotmp/$(ARCH)/boot/tach.bin
	echo 'set timeout=3' > $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	echo 'set default=0' >> $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	echo '' >> $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	echo 'menuentry "tach OS" {' >> $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	echo '    multiboot /boot/tach.bin' >> $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	echo '    boot' >> $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	echo '}' >> $(CURDIR)/isotmp/$(ARCH)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(CURDIR)/isotmp/$(ARCH) 2>/dev/null || echo "Note: grub-mkrescue not available, ISO not created"
	rm -rf $(CURDIR)/isotmp/$(ARCH)

clean:
	rm -f $(KERNEL_OBJS) $(BOOT_OBJ) $(KERNEL_BIN) $(ISO)

.PHONY: all clean
