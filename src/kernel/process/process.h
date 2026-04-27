#pragma once

#include <stdint.h>

struct process {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};

struct process * process_create(uintptr_t entry_addr, uint8_t is_umode);

void process_switch(struct process * next);
