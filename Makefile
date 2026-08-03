# tach Root Makefile

include Config.mk

HOST_CC ?= cc

.PHONY: all clean test test-host userland i686 x86_64 arm32 aarch64 riscv32 riscv64 iso

all: $(ARCH)

i686 x86_64 arm32 aarch64 riscv32 riscv64:
	@echo "Building tach for $@..."
	$(MAKE) -C src ARCH=$@ CROSS_COMPILE=$(CROSS_COMPILE)

iso:
	@echo "Creating bootable ISO for $(ARCH)..."
	@if [ -z "$(INITRD)" ] && { [ "$(ARCH)" = x86_64 ] || [ "$(ARCH)" = i686 ]; }; then \
		$(MAKE) userland ARCH=$(ARCH); \
	fi
	$(MAKE) -C src iso ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		INITRD="$(if $(strip $(INITRD)),$(INITRD),$(if $(filter $(ARCH),x86_64 i686),$(CURDIR)/src/build/$(ARCH)/user/initrd.tar,))"

userland:
	$(MAKE) -C user ARCH=$(ARCH) BUILD=$(CURDIR)/src/build/$(ARCH)/user \
		ROOTFS=$(CURDIR)/src/build/$(ARCH)/rootfs

test: test-host

test-host:
	@mkdir -p src/build/host
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -fno-builtin -DTACH_HOST_TEST -Iinclude \
		tests/host/userspace_test.c \
		src/lib/string.c src/lib/printf.c src/kernel/klog.c \
		src/fs/vfs.c src/proc/fd.c src/proc/process.c \
		src/proc/scheduler.c src/proc/syscall.c src/proc/signal.c src/proc/wait.c \
		src/sync/atomic.c src/sync/spinlock.c src/hal/cpu.c src/hal/irq.c src/hal/time.c src/hal/user.c src/hal/smp.c src/ipc/sem.c \
		src/term/tty.c src/term/vterm.c src/term/shell.c \
		src/userland/runtime.c \
		-o src/build/host/userspace-test
	@src/build/host/userspace-test
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -fno-builtin -DTACH_HOST_TEST -Iinclude \
		tests/host/mm_elf_test.c src/lib/string.c src/mm/pmm.c \
		src/proc/elf_loader.c src/sync/atomic.c src/sync/spinlock.c \
		src/hal/cpu.c src/hal/irq.c -o src/build/host/mm-elf-test
	@src/build/host/mm-elf-test
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -fno-builtin -DTACH_HOST_TEST -Iinclude \
		tests/host/vmm_fault_test.c src/lib/string.c src/mm/vmm.c \
		src/sync/atomic.c src/sync/spinlock.c src/hal/cpu.c src/hal/irq.c \
		-o src/build/host/vmm-fault-test
	@src/build/host/vmm-fault-test
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -fno-builtin -DTACH_HOST_TEST -Iinclude \
		tests/host/heap_test.c src/lib/string.c src/mm/kheap.c src/mm/slab.c \
		src/sync/atomic.c src/sync/spinlock.c src/hal/cpu.c src/hal/irq.c \
		-o src/build/host/heap-test
	@src/build/host/heap-test
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -fno-builtin -DTACH_HOST_TEST -Iinclude \
		tests/host/storage_test.c src/lib/string.c src/fs/vfs.c src/fs/fat32.c \
		src/drivers/block/block.c -o src/build/host/storage-test
	@src/build/host/storage-test
	@echo "Host userspace, PMM, VMM/ELF/fault, heap/slab, storage, preemption, process, signal, and semaphore tests passed."

clean:
	rm -rf src/build/
	find src -name "*.o" -delete
	find src -name "*.bin" -delete
