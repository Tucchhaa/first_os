#include "kthreads.h"

#include "../mm/dynamic_allocator.h"
#include "../interrupts/csr.h"
#include "task.h"
#include "cpu_scheduler.h"

#include "../../uart/uart.h"

#define KERNEL_STACK_SIZE 16384

struct task * kthread_create(void (*entry_point)(void), void * arg) {
    struct task * task = task_allocate();
    task->thread.ra = (uint64_t)entry_point;
    task->thread.sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;
    task->arg = arg;

    struct trapframe * trapframe = (struct trapframe *)task->thread.sscratch;
    trapframe->sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;

    cpu_scheduler_add_task(task);

    return task;
}

void kthread_sret() {
    struct task * current_task = get_current_task();
    struct trapframe * trapframe = (struct trapframe *)current_task->thread.sscratch;
    trapframe->sepc = (uint64_t)current_task->arg;
    trapframe->sstatus = CSR_SSTATUS_SIE;

    csr_sstatus_set(trapframe->sstatus);
    csr_sepc_set(trapframe->sepc);

    csr_sret();
}

void kthread_exit() {
    cpu_scheduler_kill();
    cpu_scheduler_next();
}
