#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

struct task * kthread_create(void (*entry_point)(void));

void kthread_exit();
