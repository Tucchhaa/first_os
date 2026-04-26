
#include "interrupts.h"

#include <stdint.h>

#include "csr.h"
#include "plic.h"
#include "../../platform.h"
#include "../../uart_sync.h"
#include "../uart/uart.h"
#include "../../string.h"
#include "../../fdt/fdt.h"
#include "../../utils.h"
#include "../sbi.h"
#include "../mm/dynamic_allocator.h"

struct cpuframe {
    uint64_t regs[32];
    uint64_t sepc;
    uint64_t sstatus;
};

static struct cpuframe * cpuframe = 0;

uint32_t cpu_frequency = 0;

void _interrupts_entry();
void _interrupts_schedule();

uint32_t _read_cpu_frequency_from_fdt() {
    uintptr_t cpus_node_addr = fdt_node_addr_by_path("/cpus");
    struct fdt_property * cpu_frequency_prop = fdt_property_by_name(cpus_node_addr, "timebase-frequency");

    if (cpu_frequency_prop == (void *)0) {
        return 0;
    }

    return be32_to_cpu(*(uint32_t *)(&cpu_frequency_prop->data));
}

void interrupts_setup() {
    uart_sync_puts("[KERNEL:INTERRUPTS] Setting up...\n");

    cpuframe = allocate(sizeof(struct cpuframe));

    uint8_t * bytes = (uint8_t *)cpuframe;
    for (uint32_t i = 0; i < sizeof(struct cpuframe); i++) {
        bytes[i] = 0;
    }

    csr_sscratch_set((uintptr_t)cpuframe);
    csr_stvec_set((uintptr_t)_interrupts_entry);

    csr_sstatus_enable(CSR_SSTATUS_SIE);

    // read cpu frequency from fdt
    cpu_frequency = _read_cpu_frequency_from_fdt();

    if (cpu_frequency == 0) {
        uart_sync_puts("[KERNEL:INTERRUPTS] /cpus[timebase-frequency] not found\n");
    } else {
        _interrupts_schedule();
    }

    uart_sync_puts("[KERNEL:INTERRUPTS] Done setting up\n");
}

void interrupts_enable() { csr_sstatus_enable(CSR_SSTATUS_SIE); }
uint64_t interrupts_disable() { return csr_sstatus_rdisable(CSR_SSTATUS_SIE); }

void interrupts_enable_external() { csr_sie_enable(CSR_SIE_SEIE); }
void interrupts_disable_external() { csr_sie_disable(CSR_SIE_SEIE); }

void interrupts_enable_timer() { csr_sie_enable(CSR_SIE_STIE); }
void interrupts_disable_timer() { csr_sie_disable(CSR_SIE_STIE); }

void interrupts_enter_umode(uintptr_t proc_addr) {
    csr_sstatus_enable(CSR_SSTATUS_SPIE);
    csr_sstatus_disable(CSR_SSTATUS_SPP);
    csr_sepc_set(proc_addr);

    csr_sret();
}

void _interrupts_external_handler();

void interrupts_handler() {
    uint64_t scause = csr_scause_get();

    switch (scause)
    {
    case ((1ULL << 63) | 5): // is_timer
        uint64_t time = sbi_read_time() / cpu_frequency;
        char c[40];
        itoa(time, c);

        uart_puts_variadic("boot time: ", c, "\n", 0);
        _interrupts_schedule();
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
        i32tox(cpuframe->sepc, sepc_buf);
        itoa(stval, stval_buf);

        uart_puts("=== S-Mode trap ===\n");
        uart_puts_variadic("scause: ", scause_buf, "\n", 0);
        uart_puts_variadic("sepc: ", sepc_buf, "\n", 0);
        uart_puts_variadic("stval: ", stval_buf, "\n", 0);

        // skip the ecall instruction
        cpuframe->sepc += 4;
        break;
    }
}

void _interrupts_schedule() {
    uint64_t target_time = sbi_read_time() + (cpu_frequency * 10); // +2 seconds
    sbi_set_timer(target_time);
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