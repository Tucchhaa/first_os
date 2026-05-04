#pragma once

#include "../ds/linked_list.h"

struct task;
union task_wait_event_arg;

void cpu_scheduler_init();

void cpu_scheduler_add_task(struct task * task);

void cpu_scheduler_kill();

void cpu_scheduler_wait(uint32_t event_id);

void cpu_scheduler_wait_arg(uint32_t event_id, union task_wait_event_arg arg);

void cpu_scheduler_fire(uint32_t event_id);

void cpu_scheduler_fire_cond(
    uint32_t event_id, 
    uint8_t (*cond)(struct task *, void *),
    void * arg
);

void cpu_scheduler_next();

void cpu_scheduler_idle();
