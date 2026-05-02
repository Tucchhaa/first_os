#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

struct task {
    struct thread {
        uint64_t s[12];
        uint64_t ra;
        uint64_t sp;
        uint64_t sscratch;
        uint64_t sstatus;
    } thread;

    uint8_t is_umode;

    struct linked_list_node node;

    uint32_t pid;
    uintptr_t kernel_sp;
    uintptr_t kernel_stack_addr;
    uintptr_t user_sp;
    uintptr_t user_stack_addr;
    uint8_t is_killed;
    void * arg;
};

struct trapframe {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};
