#include "cpu_scheduler.h"

#include <stddef.h>

#include "kthreads.h"

#include "../mm/dynamic_allocator.h"
#include "../interrupts/csr.h"

static struct linked_list ready_queue;
static struct linked_list waiting_queue;
static struct linked_list killed_queue;

static struct task * idle_task;

static struct task bootstrap;
struct task * current_task;

extern void _switch_to(struct task * prev, struct task * next);

void cpu_scheduler_init() {
    linked_list_init(&ready_queue);
    linked_list_init(&waiting_queue);
    linked_list_init(&killed_queue);

    current_task = &bootstrap;
}

struct task * _get_task_from_node(struct linked_list_node * node) {
    uintptr_t node_addr = (uintptr_t)node;
    uintptr_t task_addr = node_addr - offsetof(struct task, node);
    return (struct task *)task_addr;
}

void cpu_scheduler_add_task(struct task * task) {
    linked_list_insert(&ready_queue, &task->node);
}

void cpu_scheduler_kill() {
    current_task->is_killed = 1;
    linked_list_insert(&killed_queue, &current_task->node);
    cpu_scheduler_next();
}

void cpu_scheduler_next() {
    if (ready_queue.head == 0) {
        return;
    }

    struct task * prev = current_task;
    struct task * next = _get_task_from_node(ready_queue.head);

    if (prev->is_killed == 0) {
        linked_list_insert(&ready_queue, &prev->node);
    }

    linked_list_remove(&ready_queue, &next->node);
    
    current_task = next;

    _switch_to(prev, next);
}

void cpu_scheduler_idle() {
    while (1) {
        while (1) {
            struct linked_list_node * node = killed_queue.head;

            if (node == 0) {
                break;
            }

            struct task * task = _get_task_from_node(node);

            free((void *)task->kernel_sp);
            linked_list_remove(&killed_queue, node);
        }

        cpu_scheduler_next();
    }
}
