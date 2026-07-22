/* tach Operating System - i686 Linker Script */
/* 32-bit x86 kernel linked at 1M (0x00100000) */

ENTRY(boot_start)

SECTIONS
{
    . = 0x00100000;

    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }

    .text ALIGN(4K) :
    {
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

    __kernel_start = 0x00100000;
    __kernel_end = .;
}
