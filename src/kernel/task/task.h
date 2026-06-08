#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

union task_wait_event_arg {
    uint32_t i;
};

enum task_wait_event_id {
    TASK_WAIT_NONE,
    TASK_WAIT_PROCESS_KILL,
    TASK_WAIT_UART_READ,
    TASK_WAIT_UART_WRITE,
    TASK_WAIT_TIMEOUT
};

enum task_state {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_WAITING,
    TASK_STATE_KILLED
};

struct task {
    struct thread {
        uint64_t s[12];
        uint64_t ra;
        uint64_t sp;
        uint64_t sscratch;
        uint64_t sstatus;
        uint64_t satp;
    } thread;

    uintptr_t kernel_stack_addr;
    uintptr_t kernel_stack_top;

    struct linked_list_node node;

    uint32_t pid;
    uint8_t is_user;
    uint64_t * pgd;

    enum task_state state;
    struct wait_event {
        enum task_wait_event_id id;
        union task_wait_event_arg arg;
    } wait_event;

    struct linked_list mappings;

    struct linked_list signals_list;
    uintptr_t signal_sp;
};

struct trapframe {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};

static inline struct task * get_current_task() {
    struct task * result;
    asm volatile ("mv %0, tp" : "=r" (result) ::);
    return result;
}

struct task * task_create();

struct task * task_create_user(const char * path);

struct task * task_copy(struct task * source);

void task_free(struct task * task);
