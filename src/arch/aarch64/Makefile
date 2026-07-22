ARCH := aarch64
CROSS_COMPILE := aarch64-none-elf-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld
SRCS_C := mmu.c gic.c sysregs.c smp.c
SRCS_S := boot.S exceptions.S context.S
OBJS := $(SRCS_C:.c=.o) $(SRCS_S:.S=.o)
all: $(OBJS)
%.o: %.c
$(CC) -c $< -o $@
%.o: %.S
$(CC) -c $< -o $@
clean:
rm -f $(OBJS)
.PHONY: all clean
