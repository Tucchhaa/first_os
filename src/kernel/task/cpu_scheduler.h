#pragma once

#include "../ds/linked_list.h"

struct task;

void cpu_scheduler_init();

void cpu_scheduler_add_task(struct task * task);

void cpu_scheduler_kill();

void cpu_scheduler_next();

void cpu_scheduler_idle();
