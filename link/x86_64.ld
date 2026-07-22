/* tach Operating System - x86_64 Linker Script */
/* 64-bit x86 kernel linked at 1M (0x00100000) */

ENTRY(_start)

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

    /* Discard sections GCC/ld emit automatically that this freestanding,
     * non-PIC, no-unwind kernel has no use for. Without this, ld inserts
     * them as "orphan" sections wherever it likes in the layout, which
     * corrupts the address counter (.text has ended up back at VMA 0x0). */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
        *(.got)
        *(.got.plt)
        *(.plt)
        *(.rel.*)
        *(.rela.*)
    }
}
