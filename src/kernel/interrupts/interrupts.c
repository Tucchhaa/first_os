
#include "interrupts.h"

#include <stdint.h>

#include "csr.h"
#include "plic.h"
#include "timeouts.h"
#include "../process/process.h"
#include "../../uart_sync.h"
#include "../uart/uart.h"
#include "../../string.h"
#include "../../utils.h"

static struct process * kernel_process;

void _interrupts_entry();

void interrupts_setup() {
    uart_sync_puts("[KERNEL:INTERRUPTS] Setting up...\n");

    kernel_process = process_create(0, 0);

    csr_sscratch_set((uintptr_t)kernel_process);
    csr_stvec_set((uintptr_t)_interrupts_entry);
    csr_sstatus_enable(CSR_SSTATUS_SIE);

    timeouts_setup();

    uart_sync_puts("[KERNEL:INTERRUPTS] Done setting up\n");
}

void interrupts_enable() { csr_sstatus_enable(CSR_SSTATUS_SIE); }
uint8_t interrupts_disable() { return csr_sstatus_rdisable(CSR_SSTATUS_SIE); }

void interrupts_enable_external() { csr_sie_enable(CSR_SIE_SEIE); }
void interrupts_disable_external() { csr_sie_disable(CSR_SIE_SEIE); }

void interrupts_enable_timer() { csr_sie_enable(CSR_SIE_STIE); }
void interrupts_disable_timer() { csr_sie_disable(CSR_SIE_STIE); }

void _interrupts_external_handler();

void interrupts_handler(struct process * process) {
    uint64_t scause = csr_scause_get();

    switch (scause)
    {
    case ((1ULL << 63) | 5): // is_timer
        timeouts_interrupt_handler();
        break;
    case ((1ULL << 63) | 9): // is_external
        _interrupts_external_handler();
        break;
    default:
        register uint64_t stval;
        asm volatile ("csrr %0, stval" : "=r" (stval));

        char scause_buf[40];
        char sepc_buf[40];
        char stval_buf[40];

        itoa(scause, scause_buf);
        i32tox(process->sepc, sepc_buf);
        itoa(stval, stval_buf);

        uart_puts("=== S-Mode trap ===\n");
        uart_puts_variadic("scause: ", scause_buf, "\n", 0);
        uart_puts_variadic("sepc: ", sepc_buf, "\n", 0);
        uart_puts_variadic("stval: ", stval_buf, "\n", 0);

        // skip the ecall instruction
        process->sepc += 4;
        break;
    }
}

void _interrupts_external_handler() {
    uint32_t irq = plic_claim();

    if (irq == uart_irq) {
        uart_irq_handler();
    }

    if (irq != 0) {
        plic_complete(irq);
    }
}
