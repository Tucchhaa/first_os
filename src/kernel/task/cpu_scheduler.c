#include "cpu_scheduler.h"

#include <stddef.h>

#include "kthreads.h"
#include "task.h"
#include "task_table.h"
#include "../mm/dynamic_allocator.h"
#include "../interrupts/timeouts.h"
#include "../interrupts/interrupt_handler.h"
#include "../interrupts/interrupt_control.h"

#include "../../uart/uart.h"
#include "../../converters.h"

static struct linked_list ready_queue;
static struct linked_list waiting_queue;
static struct linked_list killed_queue;

static struct task bootstrap;
static struct task * idle_task;

static const uint32_t time_quantum = 31250; // 31250 usec = 1/32 sec
static uint32_t timeout_id;

extern void _switch_to_kernel(struct task * prev, struct task * next);

static inline void set_current_task(struct task * task) {
    asm volatile ("mv tp, %0" :: "r"(task) :);
}

void cpu_scheduler_init() {
    task_table_setup();

    linked_list_init(&ready_queue);
    linked_list_init(&waiting_queue);
    linked_list_init(&killed_queue);

    bootstrap.state = TASK_STATE_KILLED;
    set_current_task(&bootstrap);

    idle_task = kthread_create(cpu_scheduler_idle);
    cpu_scheduler_add_task(idle_task);
}

static struct task * _get_task_from_node(struct linked_list_node * node) {
    if (node == 0) {
        return 0;
    }

    uintptr_t node_addr = (uintptr_t)node;
    uintptr_t task_addr = node_addr - offsetof(struct task, node);

    return (struct task *)task_addr;
}

static void _set_need_reschedule_cpu(void *) {
    set_need_reschedule_cpu();
}

void cpu_scheduler_add_task(struct task * task) {
    uint8_t pie = interrupts_disable();

    task->state = TASK_STATE_READY;
    linked_list_insert(&ready_queue, &task->node);
    task_table_add_task(task);

    interrupts_restore(pie);
}

static uint8_t _task_pid_matches(struct task * task, void * arg) {
    return task->pid == (uint32_t)((uint64_t)arg);
}

static uint8_t _task_wait_arg_matches(struct task * task, void * arg) {
    return task->wait_event.arg.i == (uint32_t)((uint64_t)arg);
}

static void _cpu_scheduler_kill_core(struct task * task) {
    uint8_t pie = interrupts_disable();
    task->state = TASK_STATE_KILLED;

    linked_list_insert(&killed_queue, &task->node);
    interrupts_restore(pie);

    cpu_scheduler_fire_cond(
        TASK_WAIT_PROCESS_KILL, 
        _task_wait_arg_matches, 
        (void *)((uint64_t)task->pid)
    );
}

void cpu_scheduler_kill() {
    _cpu_scheduler_kill_core(get_current_task());
    cpu_scheduler_next();
}

uint8_t cpu_scheduler_kill_by_pid(uint32_t pid) {
    if (pid == get_current_task()->pid) {
        return 1;
    }

    uint8_t pie = interrupts_disable();

    struct task * task = task_table_get_task(pid);

    if (task == 0 || task->state == TASK_STATE_KILLED) {
        interrupts_restore(pie);
        return 1;
    }

    if (task->state == TASK_STATE_READY) {
        linked_list_remove(&ready_queue, &task->node);
    }
    else if (task->state == TASK_STATE_WAITING) {
        linked_list_remove(&waiting_queue, &task->node);
    }

    interrupts_restore(pie);
    _cpu_scheduler_kill_core(task);

    return 0;
}

void cpu_scheduler_wait(uint32_t event_id) {
    union task_wait_event_arg arg = { .i = 0 };
    cpu_scheduler_wait_arg(event_id, arg);
}

void cpu_scheduler_wait_arg(uint32_t event_id, union task_wait_event_arg arg) {
    uint8_t pie = interrupts_disable();

    struct task * current_task = get_current_task();
    current_task->wait_event.id = event_id;
    current_task->wait_event.arg = arg;
    current_task->state = TASK_STATE_WAITING;

    linked_list_insert(&waiting_queue, &current_task->node);

    interrupts_restore(pie);
    cpu_scheduler_next();
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
    uint8_t pie = interrupts_disable();

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

    interrupts_restore(pie);
}

void _cpu_scheduler_fire_timeout(void * arg) {
    cpu_scheduler_fire_cond(TASK_WAIT_TIMEOUT, _task_pid_matches, arg);
}

void cpu_scheduler_sleep(uint32_t usec) {
    set_timeout(
        _cpu_scheduler_fire_timeout, 
        (void *)((uint64_t)get_current_task()->pid), 
        usec
    );
    cpu_scheduler_wait(TASK_WAIT_TIMEOUT);
}

void cpu_scheduler_next() {
    if (is_handling_interrupt) {
        set_need_reschedule_cpu();
        return;
    }

    uint8_t pie = interrupts_disable();

    if (ready_queue.head == 0) {
        interrupts_restore(pie);
        return;
    }

    clear_timeout(timeout_id);
    timeout_id = set_timeout(_set_need_reschedule_cpu, 0, time_quantum);

    struct task * prev = get_current_task();
    struct task * next = _get_task_from_node(ready_queue.head);

    if (prev->state == TASK_STATE_RUNNING) {
        prev->state = TASK_STATE_READY;
        linked_list_insert(&ready_queue, &prev->node);
    }

    next->state = TASK_STATE_RUNNING;

    linked_list_remove(&ready_queue, &next->node);
    interrupts_restore(pie);

    _switch_to_kernel(prev, next);
}

void cpu_scheduler_idle() {
    while (1) {
        while (1) {
            uint8_t pie = interrupts_disable();

            struct linked_list_node * node = killed_queue.head;

            if (node == 0) {
                interrupts_restore(pie);
                break;
            }

            linked_list_remove(&killed_queue, node);
            interrupts_restore(pie);

            struct task * task = _get_task_from_node(node);
            task_table_remove_task(task);

            task_free(task);
        }

        cpu_scheduler_next();
    }
}
