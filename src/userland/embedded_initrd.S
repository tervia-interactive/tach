.section .rodata
.balign 512
.global embedded_initrd_start
.global embedded_initrd_end
embedded_initrd_start:
#if defined(__x86_64__)
    .incbin "build/x86_64/user/initrd.tar"
#elif defined(__i386__)
    .incbin "build/i686/user/initrd.tar"
#elif defined(__aarch64__)
    .incbin "build/aarch64/user/initrd.tar"
#elif defined(__arm__)
    .incbin "build/arm32/user/initrd.tar"
#elif defined(__riscv) && __riscv_xlen == 64
    .incbin "build/riscv64/user/initrd.tar"
#elif defined(__riscv)
    .incbin "build/riscv32/user/initrd.tar"
#endif
embedded_initrd_end:
.section .note.GNU-stack,"",@progbits
