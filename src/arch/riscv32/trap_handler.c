#include <kernel/types.h>
#include <hal/irq.h>
#include <kernel/signal.h>
#include <mm/vmm.h>
#include <proc/process.h>
#include <proc/scheduler.h>
#include <proc/syscall.h>
struct riscv_trap_frame { uintptr_t x[32], epc, status; };
static void to_context(const struct riscv_trap_frame* f, struct user_context* c) {
    c->pc=f->epc; c->sp=f->x[2]; c->flags=f->status;
    for(size_t i=0;i<32;i++) c->regs[i]=f->x[i];
}
static void from_context(const struct user_context* c, struct riscv_trap_frame* f) {
    for(size_t i=0;i<32;i++) f->x[i]=c->regs[i];
    f->epc=c->pc; f->x[2]=c->sp; f->status=(c->flags&~(uintptr_t)0x100u)|0x20u;
}
void riscv_trap_handler(struct riscv_trap_frame* frame) {
    uintptr_t cause,value; __asm__ volatile("csrr %0, scause":"=r"(cause));
    __asm__ volatile("csrr %0, stval":"=r"(value));
    uintptr_t interrupt=(uintptr_t)1<<(sizeof(uintptr_t)*8-1),code=cause&~interrupt;
    if(cause&interrupt){if(code==5)hal_irq_dispatch(5);return;}
    struct process* p=process_get_current(); bool user=!(frame->status&0x100u);
    if(code==8&&user&&p&&p->user_mode){
        frame->epc+=4;to_context(frame,&p->user_context);
        long result=syscall_dispatch((int)frame->x[17],frame->x[10],frame->x[11],
            frame->x[12],frame->x[13],frame->x[14],frame->x[15]);
        p=process_get_current();if(!p)return;
        if(p->exec_pending){p->exec_pending=false;p->user_context.regs[10]=0;}
        else{to_context(frame,&p->user_context);p->user_context.regs[10]=(uintptr_t)result;}
        (void)signal_deliver_pending(p,&p->user_context);
        if(p->state!=PROCESS_STATE_RUNNING)for(;;)scheduler_yield();
        from_context(&p->user_context,frame);return;
    }
    if(code==12||code==13||code==15){
        if(vmm_handle_fault(value,code==15)==0)return;
        if(user&&p&&p->user_mode){to_context(frame,&p->user_context);
            p->signal_fault_address=value;(void)signal_send(p->pid,SIGSEGV);
            (void)signal_deliver_pending(p,&p->user_context);
            if(p->state!=PROCESS_STATE_RUNNING)for(;;)scheduler_yield();
            from_context(&p->user_context,frame);}
    }
}
