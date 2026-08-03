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
else ifeq ($(ARCH),arm32)
override CC := arm-linux-gnueabihf-gcc
override LD := arm-linux-gnueabihf-ld
ARCH_CFLAGS := -march=armv7-a -marm -mfloat-abi=softfp
LD_EMULATION := armelf_linux_eabi
else ifeq ($(ARCH),aarch64)
override CC := aarch64-linux-gnu-gcc
override LD := aarch64-linux-gnu-ld
ARCH_CFLAGS := -mno-outline-atomics
LD_EMULATION := aarch64elf
else ifeq ($(ARCH),riscv32)
override CC := riscv64-linux-gnu-gcc
override LD := riscv64-linux-gnu-ld
ARCH_CFLAGS := -march=rv32imac_zicsr -mabi=ilp32 -mcmodel=medany \
               -msmall-data-limit=0
LD_EMULATION := elf32lriscv
else ifeq ($(ARCH),riscv64)
override CC := riscv64-linux-gnu-gcc
override LD := riscv64-linux-gnu-ld
ARCH_CFLAGS := -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany \
               -msmall-data-limit=0
LD_EMULATION := elf64lriscv
else
$(error Unsupported userspace architecture: $(ARCH))
endif

CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin -fno-pic -fno-pie \
          -Wall -Wextra -Werror -nostdlib -Iinclude $(ARCH_CFLAGS) -O2
CRT := crt/$(ARCH).S
PROGRAMS := sh ls cat mkdir rm touch write cp sync uname

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

$(BUILD)/%: $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/%.o user.ld
	$(LD) -m $(LD_EMULATION) -T user.ld -o $@ $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/$*.o

$(BUILD)/initrd.tar: $(BUILD)/init $(addprefix $(BUILD)/,$(PROGRAMS)) motd.txt
	@mkdir -p $(ROOTFS)/sbin $(ROOTFS)/bin $(ROOTFS)/etc
	cp $(BUILD)/init $(ROOTFS)/sbin/init
	@for program in $(PROGRAMS); do cp $(BUILD)/$$program $(ROOTFS)/bin/$$program; done
	cp motd.txt $(ROOTFS)/etc/motd
	tar --format=ustar -cf $@ -C $(ROOTFS) sbin bin etc

clean:
	rm -rf $(BUILD) $(ROOTFS)
