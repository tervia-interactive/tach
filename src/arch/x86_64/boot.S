.section .multiboot_header
.align 8

.set MULTIBOOT2_MAGIC, 0xE85250D6
.set MULTIBOOT2_ARCH, 0

.long MULTIBOOT2_MAGIC
.short MULTIBOOT2_ARCH
.long header_end - header_start
.long 0x10000

.section .bss
.align 16
stack_bottom:
.skip 32768
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %rsp
    xor %rbp, %rbp
    call kernel_main
    cli
1:  hlt
    jmp 1b

.size _start, . - _start

.section .multiboot_header
header_start:
.align 8
header_end:
