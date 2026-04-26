
#include "interrupts.h"

#include <stdint.h>

#include "../../platform.h"
#include "../../sbi.h"
#include "../../uart.h"
#include "../../string.h"
#include "csr.h"
#include "../mm/dynamic_allocator.h"
// #include "../plic/plic.h"

static struct cpuframe * cpuframe;

void _interrupts_entry();

void interrupts_setup() {
    cpuframe = allocate(sizeof(struct cpuframe));

    uint8_t * bytes = (uint8_t *)cpuframe;
    for (uint32_t i = 0; i < sizeof(struct cpuframe); i++) {
        bytes[i] = 0;
    }

    csr_sscratch_set((uintptr_t)cpuframe);
    csr_stvec_set((uintptr_t)_interrupts_entry);

    csr_sstatus_set(CSR_SSTATUS_SIE);
    // Mask interrupts, enabled timer interrupts only
    // asm volatile("csrs sie, %0" :: "r"(1 << 5));
}

void interrupts_enter_umode(uintptr_t proc_addr) {
    csr_sstatus_enable(CSR_SSTATUS_SPIE);
    csr_sstatus_disable(CSR_SSTATUS_SPP);
    csr_sepc_set(proc_addr);

    csr_sret();
}

void interrupts_handler() {
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
