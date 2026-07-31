.section .text
.code64
.global x86_64_enter_user
.type x86_64_enter_user, @function
x86_64_enter_user:
    movq %rdi, %rax
    movq 32(%rax), %rdi
    movq 40(%rax), %rsi
    movq 48(%rax), %rdx
    movq 56(%rax), %r10
    movq 64(%rax), %r8
    movq 72(%rax), %r9
    movq 80(%rax), %rbx
    movq 88(%rax), %rbp
    movq 96(%rax), %rcx
    movq 104(%rax), %r11
    movq 112(%rax), %r12
    movq 120(%rax), %r13
    movq 128(%rax), %r14
    movq 136(%rax), %r15
    pushq $0x1b
    pushq 8(%rax)
    movq 16(%rax), %r11
    orq $0x200, %r11
    pushq %r11
    pushq $0x23
    pushq 0(%rax)
    movq 24(%rax), %rax
    iretq
.size x86_64_enter_user, . - x86_64_enter_user

.section .note.GNU-stack,"",@progbits
