#include "kthreads.h"

#include "../mm/dynamic_allocator.h"
#include "../interrupts/csr.h"
#include "task.h"

extern void _switch_to_user();

struct task * kthread_create(void (*entry_point)(void), void * arg) {
    struct task * task = task_allocate();
    task->wait_event.id = TASK_WAIT_NONE;
    task->thread.ra = (uint64_t)entry_point;
    // status.spp actaully is not needed here, because kthread is never sret'ed
    task->thread.sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;
    task->arg = arg;

    return task;
}

void kthread_exec_user() {
    struct task * current_task = get_current_task();
    struct trapframe * trapframe = (struct trapframe *)current_task->thread.sscratch;
    trapframe->sepc = (uint64_t)current_task->arg;
    trapframe->sstatus = CSR_SSTATUS_SPIE;
    trapframe->regs[2] = current_task->kernel_sp; // set sp reg
    // TODO: user process should not have access to struct task
    trapframe->regs[4] = (uint64_t)current_task; // set tp reg
    // TODO: set ra reg, to make exit syscall

    csr_sscratch_set((uintptr_t)trapframe);

    _switch_to_user();
}
