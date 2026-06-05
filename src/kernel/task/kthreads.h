#pragma once

#include <stdint.h>

struct task;

struct task * kthread_create(void (*entry_point)(void));

void kthread_exec_user(struct task * task);
