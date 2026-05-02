#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

struct task {
    struct thread {
        uint64_t s[12];
        uint64_t ra;
        uint64_t sp;
    } thread;

    struct linked_list_node node;

    uint32_t pid;
    uintptr_t kernel_sp;
    uintptr_t user_sp;
    uint8_t is_killed;
};

struct trapframe {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};

struct task * kthread_create(void (*entry_point)(void));

void kthread_exit();
