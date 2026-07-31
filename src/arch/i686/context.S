.global task_switch
.type task_switch, @function
task_switch:
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl %ebp
    movl 20(%esp), %eax
    movl %esp, (%eax)
    movl 24(%esp), %eax
    movl (%eax), %esp
    popl %ebp
    popl %edi
    popl %esi
    popl %ebx
    ret
.size task_switch, . - task_switch
