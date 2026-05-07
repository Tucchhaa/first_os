#include "cpu_scheduler.h"

#include <stddef.h>

#include "kthreads.h"
#include "task.h"
#include "../mm/dynamic_allocator.h"
#include "../interrupts/timeouts.h"
#include "../interrupts/interrupts.h"

#include "../../uart/uart.h"
#include "../../converters.h"

static struct linked_list ready_queue;
static struct linked_list waiting_queue;
static struct linked_list killed_queue;

static struct task bootstrap;
static struct task * idle_task;

static const uint32_t time_quantum = 200;
static uint32_t timeout_id;

extern void _switch_to_kernel(struct task * prev, struct task * next);

static inline void set_current_task(struct task * task) {
    asm volatile ("mv tp, %0" :: "r"(task) :);
}

void cpu_scheduler_idle();

void cpu_scheduler_init() {
    linked_list_init(&ready_queue);
    linked_list_init(&waiting_queue);
    linked_list_init(&killed_queue);

    bootstrap.state = TASK_STATE_KILLED;
    set_current_task(&bootstrap);

    idle_task = kthread_create(cpu_scheduler_idle, 0);
}

static struct task * _get_task_from_node(struct linked_list_node * node) {
    if (node == 0) {
        return 0;
    }

    uintptr_t node_addr = (uintptr_t)node;
    uintptr_t task_addr = node_addr - offsetof(struct task, node);
    return (struct task *)task_addr;
}

void cpu_scheduler_add_task(struct task * task) {
    task->state = TASK_STATE_READY;
    linked_list_insert(&ready_queue, &task->node);
}

uint8_t _task_pid_matches(struct task * task, void * arg) {
    uint32_t pid = *(uint32_t *)arg;
    return task->pid == pid;
}

void cpu_scheduler_kill() {
    struct task * current_task = get_current_task();

    current_task->state = TASK_STATE_KILLED;
    linked_list_insert(&killed_queue, &current_task->node);

    cpu_scheduler_fire_cond(
        TASK_WAIT_PROCESS_KILL, _task_pid_matches, &current_task->pid
    );
}

void cpu_scheduler_wait(uint32_t event_id) {
    union task_wait_event_arg arg = { .i = 0 };
    cpu_scheduler_wait_arg(event_id, arg);
}

void cpu_scheduler_wait_arg(uint32_t event_id, union task_wait_event_arg arg) {
    struct task * current_task = get_current_task();
    current_task->wait_event.id = event_id;
    current_task->wait_event.arg = arg;
    current_task->state = TASK_STATE_WAITING;

    linked_list_insert(&waiting_queue, &current_task->node);
}

void cpu_scheduler_fire(uint32_t event_id) {
    uint8_t (*cond)(struct task *, void *) = 0;

    return cpu_scheduler_fire_cond(event_id, cond, 0);
}

void cpu_scheduler_fire_cond(
    uint32_t event_id, 
    uint8_t (*cond)(struct task *, void *),
    void * arg
) {
    struct task * task = _get_task_from_node(waiting_queue.head);

    while (task) {
        struct task * next_task = _get_task_from_node(task->node.next);

        if (task->wait_event.id == event_id) {
            uint8_t condition = cond == 0 || cond(task, arg);

            if (condition) {
                task->state = TASK_STATE_READY;
                linked_list_remove(&waiting_queue, &task->node);
                linked_list_insert(&ready_queue, &task->node);
            }
        }

        task = next_task;
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

    if (prev->state == TASK_STATE_RUNNING) {
        prev->state = TASK_STATE_READY;
        linked_list_insert(&ready_queue, &prev->node);
    }

    next->state = TASK_STATE_RUNNING;
    linked_list_remove(&ready_queue, &next->node);

    _switch_to_kernel(prev, next);
}

// TODO: if there is always some task, killed tasks will never be freed
void cpu_scheduler_idle() {
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
