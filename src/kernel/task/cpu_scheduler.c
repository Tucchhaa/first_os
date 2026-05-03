#include "cpu_scheduler.h"

#include <stddef.h>

#include "kthreads.h"
#include "task.h"
#include "../mm/dynamic_allocator.h"
#include "../interrupts/timeouts.h"
#include "../interrupts/interrupts.h"

static struct linked_list ready_queue;
static struct linked_list waiting_queue;
static struct linked_list killed_queue;

static struct task bootstrap;

static const uint32_t time_quantum = 200;
static uint32_t timeout_id;

extern void _switch_to(struct task * prev, struct task * next);

static inline void set_current_task(struct task * task) {
    asm volatile ("mv tp, %0" :: "r"(task) :);
}

void cpu_scheduler_init() {
    linked_list_init(&ready_queue);
    linked_list_init(&waiting_queue);
    linked_list_init(&killed_queue);

    bootstrap.is_killed = 1;
    set_current_task(&bootstrap);
}

static struct task * _get_task_from_node(struct linked_list_node * node) {
    uintptr_t node_addr = (uintptr_t)node;
    uintptr_t task_addr = node_addr - offsetof(struct task, node);
    return (struct task *)task_addr;
}

void cpu_scheduler_add_task(struct task * task) {
    linked_list_insert(&ready_queue, &task->node);
}

void cpu_scheduler_kill() {
    struct task * current_task = get_current_task();
    current_task->is_killed = 1;

    linked_list_insert(&killed_queue, &current_task->node);

    if (ready_queue.head == 0) {
        struct task * killed_task = current_task;
        current_task = &bootstrap;
        _switch_to(killed_task, current_task);
    } else {
        cpu_scheduler_next();
    }
}

void cpu_scheduler_next() {
    if (ready_queue.head == 0) {
        return;
    }

    clear_timeout(timeout_id);
    timeout_id = set_timeout(set_need_reschedule_cpu, 0, time_quantum);

    struct task * prev = get_current_task();
    struct task * next = _get_task_from_node(ready_queue.head);

    if (prev->is_killed == 0) {
        linked_list_insert(&ready_queue, &prev->node);
    }

    linked_list_remove(&ready_queue, &next->node);

    _switch_to(prev, next);
}

void cpu_scheduler_idle() {
    timeout_id = set_timeout(set_need_reschedule_cpu, 0, time_quantum);

    while (1) {
        while (1) {
            struct linked_list_node * node = killed_queue.head;

            if (node == 0) {
                break;
            }

            linked_list_remove(&killed_queue, node);

            struct task * task = _get_task_from_node(node);
            free((void *)task->kernel_stack_addr);
            free((void *)task);
        }

        cpu_scheduler_next();
    }
}
