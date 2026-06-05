#include "kthreads.h"

#include "../interrupts/csr.h"
#include "task.h"
#include "cpu_scheduler.h"

extern void _switch_to_user();

struct task * kthread_create(void (*entry_point)(void)) {
    struct task * task = task_create();

    task->thread.ra = (uint64_t)entry_point;
    // status.spp actually is not needed here, because kthread is never sret'ed
    task->thread.sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP | CSR_SSTATUS_SUM;

    return task;
}

void kthread_exec_user(struct task * task) {
    struct trapframe * trapframe = (struct trapframe *)task->thread.sscratch;

    task->thread.ra = (uint64_t)_switch_to_user;
    task->thread.sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;

    // TODO: sstatus.SUM allows user pointer dereference, which can be exploited
    trapframe->sstatus = CSR_SSTATUS_SPIE | CSR_SSTATUS_SUM;

    cpu_scheduler_add_task(task);
}
