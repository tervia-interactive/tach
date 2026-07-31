# i686 Boot Entry Point
# Multiboot 1 compliant bootloader entry

.section .multiboot, "a"
.align 4
.long 0x1BADB002
.long 0x00000000
.long -(0x1BADB002 + 0x00000000)

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    # Multiboot 1 enters with EAX=magic and EBX=information pointer.
    # Pass both through the C ABI instead of discarding the memory map.
    pushl %ebx
    pushl %eax
    call kernel_main
    cli
1:  hlt
    jmp 1b

.size _start, . - _start
