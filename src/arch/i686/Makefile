# i686 Architecture Makefile

ARCH := i686
CROSS_COMPILE := i686-elf-
CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
LD := $(CROSS_COMPILE)ld

SRCS_C := gdt.c idt.c isr.c irq.c ports.c paging.c smp.c
SRCS_S := boot.S context.S
OBJS := $(SRCS_C:.c=.o) $(SRCS_S:.S=.o)

all: $(OBJS)

%.o: %.c
$(CC) -c $< -o $@

%.o: %.S
$(CC) -c $< -o $@

clean:
rm -f $(OBJS)

.PHONY: all clean
