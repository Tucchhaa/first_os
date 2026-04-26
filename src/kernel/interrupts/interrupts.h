#pragma once

#include <stdint.h>

struct cpuframe {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};

void interrupts_setup();

void interrupts_enter_umode(uintptr_t proc_addr);

void interrupts_handler();
