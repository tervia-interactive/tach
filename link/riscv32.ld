/* tach - RISC-V 32-bit Linker Script */
/* OpenSBI occupies 0x80000000..0x801fffff; its payload starts at 0x80200000. */

ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

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

    __kernel_start = 0x80200000;
    __kernel_end = .;
}
