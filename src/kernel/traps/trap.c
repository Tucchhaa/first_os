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
STIE - timer interrupts enabled
*/


#include "trap.h"

#include <stdint.h>

#include "../../platform.h"
#include "../../sbi.h"
#include "../../uart.h"
#include "../../string.h"
#include "../mm/dynamic_allocator.h"

static struct cpuframe * cpuframe;

void _trap_entry();

uint32_t timer_freq;

void trap_setup(uint32_t _timer_freq) {
    timer_freq = _timer_freq;

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

    // Enable interrupts
    asm volatile ("csrw sstatus, %0" :: "r" (1 << 1) : "memory");
    // Mask interrupts, enabled timer interrupts only
    asm volatile("csrs sie, %0" :: "r"(1 << 5));
}

void trap_handler() {
    register uint64_t scause;
    asm volatile ("csrr %0, scause" : "=r" (scause));

    uint8_t is_timer = (scause == ((1ULL << 63) | 5));

    if (is_timer) {
        uint64_t t;
        asm volatile ("rdtime %0" : "=r"(t));

        uint64_t time = t / timer_freq;
        char c[40];
        itoa(time, c);

        uart_puts_variadic("boot time: ", c, "\n", 0);
        schedule_interrupt();
        return;
    }

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

void schedule_interrupt() {
    uint64_t t;
    asm volatile ("rdtime %0" : "=r"(t));

    uint64_t target_time = t + (timer_freq * 2); // 2 seconds

    sbi_set_timer(target_time);
}
