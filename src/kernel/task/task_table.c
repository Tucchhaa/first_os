#include "task_table.h"

#include "task.h"
#include "../ds/linked_list.h"
#include "../mm/dynamic_allocator.h"
#include "../interrupts/interrupt_control.h"

struct entry {
    struct linked_list_node node;
    struct task * task;
};

static struct linked_list entries;

void task_table_setup() {
    linked_list_init(&entries);
}

static struct entry * _task_manager_get_entry(uint32_t pid) {
    struct entry * entry = (struct entry *)entries.head;

    while (entry) {
        if (entry->task->pid == pid) {
            return entry;
        }

        entry = (struct entry *)entry->node.next;
    }

    return 0;
} 

struct task * task_table_get_task(uint32_t pid) {
    uint8_t pie = interrupts_disable();
    struct entry * entry = _task_manager_get_entry(pid);
    interrupts_restore(pie);
    return entry == 0 ? 0 : entry->task;
}

void task_table_add_task(struct task * task) {
    uint8_t pie = interrupts_disable();
    struct entry * entry = (struct entry *)allocate(sizeof(struct entry));
    entry->task = task;

    linked_list_insert(&entries, &entry->node);
    interrupts_restore(pie);
}

void task_table_remove_task(struct task * task) {
    uint8_t pie = interrupts_disable();
    struct entry * entry = _task_manager_get_entry(task->pid);

    if (entry == 0) {
        interrupts_restore(pie);
        return;
    }

    linked_list_remove(&entries, &entry->node);
    interrupts_restore(pie);
    free((void *)entry);
}
