#include "interrupt_tasks.h"

#include "../ds/linked_list.h"
#include "interrupt_control.h"

#define TASKS_BUFFER_SIZE 128

struct interrupt_task {
    struct linked_list_node node;
    void (*handler)(void *);
    void * arg;
    uint8_t priority;
};

static struct interrupt_task tasks_buffer[TASKS_BUFFER_SIZE];
static uint32_t tasks_buffer_head = 0;

static struct linked_list tasks_queue;

void interrupt_tasks_setup() {
    linked_list_init(&tasks_queue);
}

void interrupt_tasks_add(
    void (*handler)(void *), 
    void * arg, 
    uint8_t priority
) {
    uint8_t pie = interrupts_disable();

    struct interrupt_task * new_entry = &tasks_buffer[tasks_buffer_head];
    new_entry->handler = handler;
    new_entry->arg = arg;
    new_entry->priority = priority;

    tasks_buffer_head = (tasks_buffer_head + 1) % TASKS_BUFFER_SIZE;

    struct interrupt_task * current_entry = (struct interrupt_task *)tasks_queue.head;

    while (current_entry != 0) {
        if (new_entry->priority < current_entry->priority) {
            linked_list_insert_before(&tasks_queue, &new_entry->node, &current_entry->node);
            interrupts_restore(pie);
            return;
        }

        current_entry = (struct interrupt_task *)current_entry->node.next;
    }

    linked_list_insert(&tasks_queue, &new_entry->node);
    interrupts_restore(pie);
}

uint8_t interrupt_tasks_execute() {
    uint8_t pie = interrupts_disable();

    if (tasks_queue.head == (void *)0) {
        interrupts_restore(pie);
        return 0;
    }

    struct interrupt_task * task = (struct interrupt_task *)tasks_queue.head;

    linked_list_remove(&tasks_queue, &task->node);
    interrupts_restore(pie);

    task->handler(task->arg);
    
    return 1;
}
