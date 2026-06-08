#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

struct task;

struct signal {
    struct linked_list_node node;
    uint32_t signum;
    uint8_t is_pending;
    void (*handler)();
};

void task_copy_signals(struct task * dest, struct task * source);

void task_register_signal(
    struct task * task, uint32_t signum, void (*handler)()
);

struct signal * task_get_signal(struct task * task, uint32_t signum);

struct signal * task_get_next_pending_signal(struct task * task);

void task_free_signals(struct task * task);
