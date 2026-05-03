#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

struct task;

struct task * kthread_create(void (*entry_point)(void), void * arg);

void kthread_sret();

void kthread_exit();
