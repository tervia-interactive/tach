.global task_switch
.type task_switch, %function
task_switch:
    stmfd sp!, {r4-r12, lr}
    str sp, [r0]
    ldr sp, [r1]
    ldmfd sp!, {r4-r12, pc}
.size task_switch, . - task_switch
