#include "task.h"

#include "../mm/dynamic_allocator.h"

#define KERNEL_STACK_SIZE 16384

static uint32_t created_task_count = 0;

struct task * task_allocate() {
    struct task * task = allocate(sizeof(struct task));
    
    task->node.next = task->node.prev = 0;
    task->pid = ++created_task_count;
    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_sp = task->kernel_stack_addr + KERNEL_STACK_SIZE;
    task->user_stack_addr = 0;
    task->user_sp = 0;
    task->state = 0;
    task->wait_event.id = 0;
    task->wait_event.arg.i = 0;
    task->arg = 0;

    // Init task.trapframe
    task->kernel_sp -= sizeof(struct trapframe);
    struct trapframe * trapframe = (struct trapframe *)task->kernel_sp;

    for (uint32_t i=0; i < 32; i++) {
        trapframe->regs[i] = 0;
    }

    trapframe->sepc = 0;
    trapframe->sstatus = 0;

    // Init task.thread
    for (uint32_t i=0; i < 12; i++) {
        task->thread.s[i] = 0;
    }

    task->thread.ra = 0;
    task->thread.sp = task->kernel_sp;
    task->thread.sscratch = (uint64_t)trapframe;
    task->thread.sstatus = 0;

    return task;
}

static inline uintptr_t _rebase_stack_addr(
    struct task * task,
    struct task * source,
    uintptr_t source_addr
) {
    return task->kernel_stack_addr + (source_addr - source->kernel_stack_addr);
}

struct task * task_copy(struct task * source) {
    struct task * task = allocate(sizeof(struct task));

    task->node.next = task->node.prev = 0;
    task->pid = ++created_task_count;
    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_sp = _rebase_stack_addr(task, source, source->kernel_sp);
    task->user_stack_addr = 0;
    task->user_sp = 0;
    task->state = 0;
    task->wait_event.id = 0;
    task->wait_event.arg.i = 0;
    task->arg = source->arg;

    // Copy stack
    for (uint32_t i=0; i < KERNEL_STACK_SIZE; i++) {
        *((char *)(task->kernel_stack_addr + i)) = *((char *)(source->kernel_stack_addr + i));
    }
    
    // Copy task.thread
    task->thread.ra = source->thread.ra;
    task->thread.sp = task->kernel_sp;
    task->thread.sscratch = _rebase_stack_addr(task, source, source->thread.sscratch);
    task->thread.sstatus = source->thread.sstatus;

    for (uint32_t i=0; i < 12; i++) {
        task->thread.s[i] = source->thread.s[i];
    }

    // Update trapframe addresses
    struct trapframe * source_trapframe = (struct trapframe *)source->thread.sscratch;
    struct trapframe * trapframe = (struct trapframe *)task->thread.sscratch;

    trapframe->regs[2] = _rebase_stack_addr(task, source, source_trapframe->regs[2]);
    trapframe->regs[4] = (uint64_t)task; // set tp reg

    return task;
}
