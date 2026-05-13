#pragma once

#include <stdint.h>

void task_table_setup();

struct task * task_table_get_task(uint32_t pid);

void task_table_add_task(struct task * task);

void task_table_remove_task(struct task * task);
