/* tach Operating System - RISC-V 32-bit Linker Script */
/* RISC-V kernel linked at 0x80000000 (typical load address) */

ENTRY(_start)

SECTIONS
{
    . = 0x80000000;

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

    __kernel_start = 0x80000000;
    __kernel_end = .;
}
