OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = 0x80000;

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
