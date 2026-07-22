# Tach OS Source Makefile

ARCH ?= x86_64
CROSS_COMPILE ?=
CURDIR := $(shell pwd)
ROOTDIR := $(CURDIR)/..

include $(ROOTDIR)/Config.mk

SRC_DIRS := kernel sync ipc hal boot hw mm proc fs term lib
SRC_DIRS += drivers/char drivers/video drivers/input drivers/block
SRC_DIRS += arch/$(ARCH)

C_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
ASM_SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.S))

OBJECTS := $(C_SOURCES:.c=.o) $(ASM_SOURCES:.S=.o)

all: $(BUILD_DIR) $(KERNEL_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

iso: $(KERNEL_BIN)
ifeq ($(filter $(ARCH),i686 x86_64),)
	@echo "ISO creation only supported for i686 and x86_64"
else
	mkdir -p $(BUILD_DIR)/iso/boot/grub
	cp $(KERNEL_BIN) $(BUILD_DIR)/iso/boot/tach.bin
	echo "set timeout=3" > $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "set default=0" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "menuentry \"tach OS\" {" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "  multiboot /boot/tach.bin" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "  boot" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	echo "}" >> $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(BUILD_DIR)/iso 2>/dev/null || true
endif

clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete
	find . -name "*.bin" -delete

.PHONY: all clean iso
