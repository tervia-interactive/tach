.section .text
.code32
.global i686_enter_user
.type i686_enter_user, @function
i686_enter_user:
    movl 4(%esp), %eax
    movw $0x23, %dx
    movw %dx, %ds
    movw %dx, %es
    pushl $0x23
    pushl 4(%eax)
    movl 8(%eax), %edx
    orl $0x200, %edx
    pushl %edx
    pushl $0x1b
    pushl 0(%eax)
    movl 16(%eax), %ebx
    movl 20(%eax), %ecx
    movl 24(%eax), %edx
    movl 28(%eax), %esi
    movl 32(%eax), %edi
    movl 36(%eax), %ebp
    movl 12(%eax), %eax
    iret
.size i686_enter_user, . - i686_enter_user

.section .note.GNU-stack,"",@progbits
