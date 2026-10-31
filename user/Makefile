ARCH ?= x86_64
BUILD ?= ../src/build/$(ARCH)/user
ROOTFS ?= ../src/build/$(ARCH)/rootfs
CC ?= gcc
LD ?= ld

# -mgeneral-regs-only: funcoes variadicas (printf) nao tocam em registradores
# SSE, entao nao dependem do kernel habilitar SSE nem do alinhamento da pilha.
ifeq ($(ARCH),x86_64)
ARCH_CFLAGS := -m64 -mno-red-zone -mcmodel=small -mgeneral-regs-only
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

# Utilitarios externos (user/bin/<nome>.c -> /bin/<nome>). Para nomes que o
# shell tambem tem como builtin (cat, echo), o builtin vence na linha de
# comando; a versao externa serve para pipelines, xargs e execucao direta.
BINS := head tail wc cp mv grep hexdump tee seq basename dirname stat nl rev \
        sort uniq cut tr tac find du free xargs cat echo \
        mkdir rmdir sync mount umount

LIBOBJS := $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/ext.o
HDRS := include/tach.h include/tachc.h

.PHONY: all clean
all: $(BUILD)/initrd.tar

$(BUILD)/libc.o: libc/libc.c include/tach.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ext.o: libc/ext.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/crt.o: $(CRT)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/bin_%.o: bin/%.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/init: $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/init.o user.ld
	$(LD) -m $(LD_EMULATION) -T user.ld -o $@ $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/init.o

$(BUILD)/sh: $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/sh.o user.ld
	$(LD) -m $(LD_EMULATION) -T user.ld -o $@ $(BUILD)/crt.o $(BUILD)/libc.o $(BUILD)/sh.o

$(BUILD)/bin_%: $(BUILD)/bin_%.o $(LIBOBJS) user.ld
	$(LD) -m $(LD_EMULATION) -T user.ld -o $@ $(LIBOBJS) $<

.SECONDARY: $(patsubst %,$(BUILD)/bin_%.o,$(BINS))

$(BUILD)/initrd.tar: $(BUILD)/init $(BUILD)/sh $(patsubst %,$(BUILD)/bin_%,$(BINS))
	@mkdir -p $(ROOTFS)/sbin $(ROOTFS)/bin
	cp $(BUILD)/init $(ROOTFS)/sbin/init
	cp $(BUILD)/sh $(ROOTFS)/bin/sh
	for b in $(BINS); do cp $(BUILD)/bin_$$b $(ROOTFS)/bin/$$b || exit 1; done
	tar --format=ustar -cf $@ -C $(ROOTFS) sbin bin

clean:
	rm -rf $(BUILD) $(ROOTFS)
