/* tach - AArch64 Linker Script */
/* QEMU virt RAM starts at 0x40000000 and loads the kernel at +0x80000. */

ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text ALIGN(4K) :
    {
        KEEP(*(.text.boot))
        *(.text*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }

    .data ALIGN(4K) :
    {
        *(.data*)
    }

    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    __kernel_start = 0x40080000;
    __kernel_end = .;
}
