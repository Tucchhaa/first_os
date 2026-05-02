#include "kthreads.h"

#include "../mm/dynamic_allocator.h"
#include "../interrupts/csr.h"
#include "task.h"
#include "cpu_scheduler.h"

#define KERNEL_STACK_SIZE 4096

static uint32_t created_task_count = 0;

struct task * kthread_create(void (*entry_point)(void)) {
    struct task * task = allocate(sizeof(struct task));

    task->pid = created_task_count++;
    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_sp = task->kernel_stack_addr + KERNEL_STACK_SIZE;
    task->user_stack_addr = 0;
    task->user_sp = 0;
    task->is_killed = 0;

    for (uint32_t i=0; i < 12; i++) {
        task->thread.s[i] = 0;
    }

    task->kernel_sp -= sizeof(struct trapframe);
    struct trapframe * trapframe = (struct trapframe *)task->kernel_sp;

    for (uint32_t i=0; i < 32; i++) {
        trapframe->regs[i] = 0;
    }
    trapframe->sepc = 0;
    trapframe->sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;

    task->thread.sscratch = (uint64_t)trapframe;
    task->thread.ra = (uint64_t)entry_point;
    task->thread.sp = task->kernel_sp;
    task->thread.sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;

    cpu_scheduler_add_task(task);

    return task;
}

void kthread_exit() {
    cpu_scheduler_kill();
}
