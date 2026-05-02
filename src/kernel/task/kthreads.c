#include "kthreads.h"

#include "../mm/dynamic_allocator.h"
#include "../interrupts/csr.h"
#include "cpu_scheduler.h"

#define KERNEL_STACK_SIZE 4096

static uint32_t created_task_count = 0;

struct task * kthread_create(void (*entry_point)(void)) {
    struct task * task = allocate(sizeof(struct task));

    task->pid = created_task_count++;
    task->kernel_sp = (uintptr_t)allocate(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    task->user_sp = 0;
    task->is_killed = 0;

    task->thread.ra = (uint64_t)entry_point;
    task->thread.sp = task->kernel_sp;

    for (uint32_t i=0; i < 12; i++) {
        task->thread.s[i] = 0;
    }

    cpu_scheduler_add_task(task);

    return task;
}

void kthread_exit() {
    cpu_scheduler_kill();
}
