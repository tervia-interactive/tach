# RISC-V 64-bit Architecture Makefile

ARCH := riscv64
CROSS_COMPILE := riscv64-elf-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld

SRCS := boot.S traps.S mmu.c plic.c smp.c context.S
OBJS := $(SRCS:.S=.o)
OBJS := $(OBJS:.c=.o)

all: $(OBJS)

%.o: %.S
	$(CC) -c $< -o $@

%.o: %.c
	$(CC) -c $< -o $@

clean:
	rm -f $(OBJS)

.PHONY: all clean
