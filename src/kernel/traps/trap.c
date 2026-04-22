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


#include "trap.h"

#include <stdint.h>

#include "../../uart.h"
#include "../../string.h"
#include "../mm/dynamic_allocator.h"

static struct cpuframe * cpuframe;

void _trap_entry();

void trap_setup() {
    cpuframe = allocate(sizeof(struct cpuframe));

    uint8_t * bytes = (uint8_t *)cpuframe;
    for (uint32_t i = 0; i < sizeof(struct cpuframe); i++) {
        bytes[i] = 0;
    }

    asm volatile (
        "csrw sscratch, %0"
        :
        : "r" (cpuframe)
        : "memory"
    );

    // last two bits used for mode: 0 - direct, 1 - vectored
    uint64_t stvec = (uint64_t)_trap_entry & ~3;

    asm volatile (
        "csrw stvec, %0"
        :
        : "r" (stvec)
        : "memory"
    );
}

void trap_handler() {
    register uint64_t scause;
    asm volatile ("csrr %0, scause" : "=r" (scause));

    register uint64_t stval;
    asm volatile ("csrr %0, stval" : "=r" (stval));

    char scause_buf[40];
    char sepc_buf[40];
    char stval_buf[40];

    itoa(scause, scause_buf);
    i32tox(cpuframe->sepc, sepc_buf);
    itoa(stval, stval_buf);

    uart_puts("=== S-Mode trap ===\n");
    uart_puts_variadic("scause: ", scause_buf, "\n", 0);
    uart_puts_variadic("sepc: ", sepc_buf, "\n", 0);
    uart_puts_variadic("stval: ", stval_buf, "\n", 0);

    // skip the ecall instruction
    cpuframe->sepc += 4;
}
