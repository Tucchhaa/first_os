/*
sstatus
sstatus.SPP - stores CPU-mode before the trap
sstatus.SIE - whether interrupts are enabled
sstatus.SPIE - Saves the old SIE value when a trap occurs

stvec - points to trap handler. On any trap CPU jumps here.

sepc - on a trap CPU saved PC of the interrupts instruction,
it can be used to return control to the application

scause - top bit indicates if trap happened by interrupt or exception

stval - extra context about a trap

sscractch - kernel can use however it wants

sie - mask for interrupts
*/
#pragma once

#include <stdint.h>

struct cpuframe {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};

void trap_setup();

void trap_handler();
