# x86_64 Boot Entry Point
# Multiboot2-compliant header + 32-bit -> 64-bit long mode bootstrap.
#
# Multiboot hands control to the kernel in 32-bit protected mode even for
# a 64-bit ELF kernel (this is true regardless of the kernel's own
# bitness). This file therefore starts in .code32, builds minimal
# identity-mapped page tables, enables PAE + long mode (EFER.LME) +
# paging, loads a temporary 64-bit GDT, and far-jumps into .code64 before
# ever touching a 64-bit register or calling kernel_main.

.section .multiboot, "a"
.align 8
header_start:
.long 0xE85250D6                                       # magic
.long 0                                                 # architecture: i386 protected mode
.long header_end - header_start                         # header length
.long -(0xE85250D6 + 0 + (header_end - header_start))   # checksum

# Mandatory end tag - without this GRUB rejects the header as malformed.
.align 8
.short 0
.short 0
.long 8
header_end:

.section .bss
.align 4096
pml4_table:
.skip 4096
pdpt_table:
.skip 4096
pd_table:
.skip 4096

.align 16
stack_bottom:
.skip 32768
stack_top:

.section .data
.align 8
boot_magic:
    .long 0
boot_info:
    .long 0

.align 16
gdt64:
    .quad 0                                                     # null descriptor
gdt64_code = . - gdt64
    .quad (1<<44) | (1<<47) | (1<<41) | (1<<43) | (1<<53)       # present, S=1, RW, exec, long-mode
gdt64_data = . - gdt64
    .quad (1<<44) | (1<<47) | (1<<41)                           # present, S=1, writable
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    mov $stack_top, %esp
    # Preserve the Multiboot 2 handoff before the paging bootstrap uses
    # EAX/EBX as scratch registers.
    movl %eax, boot_magic
    movl %ebx, boot_info

    # Identity-map the first 1GiB with 2MiB pages: covers the kernel
    # (linked at 1MiB), the multiboot info blob GRUB left behind, and
    # legacy I/O/VGA memory, all in one PD.
    movl $pdpt_table, %eax
    orl $0x03, %eax                # present | writable
    movl %eax, (pml4_table)

    movl $pd_table, %eax
    orl $0x03, %eax
    movl %eax, (pdpt_table)

    movl $0, %ecx
fill_pd:
    movl $0x200000, %eax
    mul %ecx
    orl $0x83, %eax                # present | writable | PS (2MiB page)
    movl %eax, pd_table(,%ecx,8)
    incl %ecx
    cmpl $512, %ecx
    jne fill_pd

    # Enable PAE (required before entering long mode)
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    # Point CR3 at the PML4
    movl $pml4_table, %eax
    movl %eax, %cr3

    # Set EFER.LME (long mode enable)
    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    # Enable paging (this activates long mode now that LME + PAE are set)
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0

    lgdt (gdt64_pointer)
    ljmp $gdt64_code, $long_mode_start

.code64
long_mode_start:
    movw $gdt64_data, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    mov $stack_top, %rsp
    xor %rbp, %rbp
    movl boot_magic(%rip), %edi
    movl boot_info(%rip), %esi
    call kernel_main
    cli
1:  hlt
    jmp 1b

.size _start, . - _start
.section .note.GNU-stack,"",@progbits
