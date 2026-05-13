#include "task.h"

#include "../mm/dynamic_allocator.h"
#include "../mm/utils.h"
#include "../interrupts/interrupt_control.h"

#define KERNEL_STACK_SIZE 16384
#define SIGNAL_STACK_SIZE 4096

static uint32_t get_next_pid() {
    uint8_t pie = interrupts_disable();
    static uint32_t next_pid = 1;
    uint32_t result = next_pid++;
    interrupts_restore(pie);
    return result;
}

struct task * task_allocate() {
    struct task * task = allocate(sizeof(struct task));
    
    task->node.next = task->node.prev = 0;
    task->pid = get_next_pid();
    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_sp = task->kernel_stack_addr + KERNEL_STACK_SIZE;
    task->user_stack_addr = 0;
    task->user_sp = 0;
    task->state = 0;
    task->wait_event.id = 0;
    task->wait_event.arg.i = 0;
    task->arg = 0;

    // Init task.signals
    linked_list_init(&task->signals_list);

    task->signal_stack_addr = (uintptr_t)allocate(SIGNAL_STACK_SIZE);
    task->signal_sp = task->signal_stack_addr + SIGNAL_STACK_SIZE;

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

void task_free(struct task * task) {
    free((void *)task->kernel_stack_addr);
    free((void *)task->signal_stack_addr);

    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        struct signal * next = (struct signal *)signal->node.next;
        free(signal);
        signal = next;
    }

    free((void *)task);
}

void task_register_signal(
    struct task * task, uint32_t signum, void (*handler)()
) {
    struct signal * signal = (struct signal *)allocate(sizeof(struct signal));

    signal->signum = signum;
    signal->handler = handler;
    signal->is_pending = 0;

    linked_list_insert(&task->signals_list, &signal->node);
}

struct signal * task_get_next_pending_signal(struct task * task) {
    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        if (signal->is_pending) {
            return signal;
        }

        signal = (struct signal *)signal->node.next;
    }

    return 0;
}

struct signal * task_get_signal(struct task * task, uint32_t signum) {
    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        if (signal->signum == signum) {
            return signal;
        }

        signal = (struct signal *)signal->node.next;
    }

    return 0;
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
    task->pid = get_next_pid();
    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_sp = _rebase_stack_addr(task, source, source->kernel_sp);
    task->user_stack_addr = 0;
    task->user_sp = 0;
    task->state = 0;
    task->wait_event.id = 0;
    task->wait_event.arg.i = 0;
    task->arg = source->arg;

    // Copy signals
    linked_list_init(&task->signals_list);

    task->signal_stack_addr = (uintptr_t)allocate(SIGNAL_STACK_SIZE);
    task->signal_sp = task->signal_stack_addr + SIGNAL_STACK_SIZE;

    struct signal * signal = (struct signal *)source->signals_list.head;

    while (signal) {
        struct signal * signal_copy = (struct signal *)allocate(sizeof(struct signal));
        signal_copy->signum = signal->signum;
        signal_copy->handler = signal->handler;
        signal_copy->is_pending = signal->is_pending;
        linked_list_insert(&task->signals_list, &signal_copy->node);

        signal = (struct signal *)signal->node.next;
    }

    memcopy((void *)task->signal_stack_addr, (void *)source->signal_stack_addr, SIGNAL_STACK_SIZE);

    // Copy stack
    memcopy((void *)task->kernel_stack_addr, (void *)source->kernel_stack_addr, KERNEL_STACK_SIZE);
    
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
