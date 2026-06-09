#include "kthreads.h"

#include "../interrupts/csr.h"
#include "task.h"
#include "cpu_scheduler.h"

struct task * kthread_create(void (*entry_point)(void)) {
    struct task * task = task_create();
    task->thread.ra = (uint64_t)entry_point;
    return task;
}