.global task_switch
.type task_switch, @function

task_switch:
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    movl 44(%esp), %eax
    movl %esp, (%eax)
    
    movl 44(%esp), %eax
    movl (%eax), %esp
    
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    add $4, %esp
    iret

.size task_switch, . - task_switch
