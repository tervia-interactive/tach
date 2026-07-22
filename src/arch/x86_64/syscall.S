.global syscall_handler
.type syscall_handler, @function
syscall_handler:
    swapgs
    movq %rsp, %gs:0
    movq %gs:32768, %rsp
    iretq
.size syscall_handler, . - syscall_handler
