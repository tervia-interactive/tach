/* tach - ARM 32-bit Linker Script */
/* ARM kernel linked at 0x00010000 (typical load address) */

ENTRY(_start)

SECTIONS
{
    . = 0x00010000;

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

    __kernel_start = 0x00010000;
    __kernel_end = .;
}
