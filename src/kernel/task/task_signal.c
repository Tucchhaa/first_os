#include "task_signal.h"

#include "task.h"
#include "../mm/dynamic_allocator.h"

void task_copy_signals(struct task * dest, struct task * source) {
    struct signal * signal = (struct signal *)source->signals_list.head;

    while (signal) {
        struct signal * signal_copy = (struct signal *)allocate(sizeof(struct signal));
        signal_copy->signum = signal->signum;
        signal_copy->handler = signal->handler;
        signal_copy->is_pending = signal->is_pending;
        linked_list_insert(&dest->signals_list, &signal_copy->node);

        signal = (struct signal *)signal->node.next;
    }

    dest->signal_sp = source->signal_sp;
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

struct signal * task_get_signal(struct task * task, uint32_t signum) {
    if (!task->is_user) {
        return 0;
    }

    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        if (signal->signum == signum) {
            return signal;
        }

        signal = (struct signal *)signal->node.next;
    }

    return 0;
}

struct signal * task_get_next_pending_signal(struct task * task) {
    if (!task->is_user) {
        return 0;
    }

    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        if (signal->is_pending) {
            return signal;
        }

        signal = (struct signal *)signal->node.next;
    }

    return 0;
}
