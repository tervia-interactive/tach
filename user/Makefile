ARCH ?= x86_64
BUILD ?= ../src/build/$(ARCH)/user
ROOTFS ?= ../src/build/$(ARCH)/rootfs
CC ?= gcc
LD ?= ld

ifeq ($(ARCH),x86_64)
ARCH_CFLAGS := -m64 -mno-red-zone -mcmodel=small
LD_EMULATION := elf_x86_64
else ifeq ($(ARCH),i686)
ARCH_CFLAGS := -m32 -mno-mmx -mno-sse
LD_EMULATION := elf_i386
else
$(error Userspace isolation is currently supported only on x86_64 and i686)
endif

CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin -fno-pic -fno-pie \
          -Wall -Wextra -Werror -nostdlib -Iinclude $(ARCH_CFLAGS) -O2
CRT := crt/$(ARCH).S

.PHONY: all clean
all: $(BUILD)/initrd.tar

$(BUILD)/libc.o: libc/libc.c include/tach.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crt.o: $(CRT)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c include/tach.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/init: $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/init.o user.ld
	$(LD) -m $(LD_EMULATION) -T user.ld -o $@ $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/init.o

$(BUILD)/sh: $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/sh.o user.ld
	$(LD) -m $(LD_EMULATION) -T user.ld -o $@ $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/sh.o

$(BUILD)/initrd.tar: $(BUILD)/init $(BUILD)/sh
	@mkdir -p $(ROOTFS)/sbin $(ROOTFS)/bin
	cp $(BUILD)/init $(ROOTFS)/sbin/init
	cp $(BUILD)/sh $(ROOTFS)/bin/sh
	tar --format=ustar -cf $@ -C $(ROOTFS) sbin bin

clean:
	rm -rf $(BUILD) $(ROOTFS)
