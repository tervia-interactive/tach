OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text :
    {
        *(.text.boot)
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}
