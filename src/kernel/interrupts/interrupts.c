
#include "interrupts.h"

#include <stdint.h>

#include "csr.h"
#include "plic.h"
#include "timeouts.h"
#include "syscalls.h"
#include "interrupt_tasks.h"
#include "../task/task.h"
#include "../task/kthreads.h"
#include "../task/cpu_scheduler.h"
#include "../../uart/uart_sync.h"
#include "../../uart/uart.h"
#include "../../string.h"
#include "../../converters.h"

static uint8_t _need_reschedule_cpu = 0;



void set_need_reschedule_cpu(void *) { 
    _need_reschedule_cpu = 1; 
}

void _interrupts_entry();

void interrupts_setup() {
    uart_sync_puts("[KERNEL:INTERRUPTS] Setting up...\n");

    interrupt_tasks_setup();
    timeouts_setup();
    csr_stvec_set((uintptr_t)_interrupts_entry);

    uart_sync_puts("[KERNEL:INTERRUPTS] Done setting up\n");
}

void interrupts_enable() { csr_sstatus_enable(CSR_SSTATUS_SIE); }
void interrupts_restore(uint8_t pie) { if (pie) csr_sstatus_enable(CSR_SSTATUS_SIE); }
uint8_t interrupts_disable() { return csr_sstatus_rdisable(CSR_SSTATUS_SIE); }

void interrupts_enable_external() { csr_sie_enable(CSR_SIE_SEIE); }
void interrupts_disable_external() { csr_sie_disable(CSR_SIE_SEIE); }

void interrupts_enable_timer() { csr_sie_enable(CSR_SIE_STIE); }
void interrupts_disable_timer() { csr_sie_disable(CSR_SIE_STIE); }

void _interrupts_external_handler(void * arg);

static void _interrupts_get_handler(
    void (**handler)(void *),
    void ** arg,
    uint8_t * priority,
    struct trapframe * trapframe
) {
    uint64_t scause = csr_scause_get();

    switch (scause)
    {
    case ((1ULL << 63) | 5): // is_timer
        timeouts_postpone();
        *priority = 1;
        *arg = 0;
        *handler = &timeouts_interrupt_handler;
        break;
    case ((1ULL << 63) | 9): // is_external
        *priority = 1;
        *arg = (void *)plic_claim();
        *handler = &_interrupts_external_handler;
        break;
    default:
        *priority = 1;
        *arg = trapframe;
        *handler = &syscall_handler;
        break;
    }
}

void interrupts_handler(struct trapframe * trapframe) {
    static uint8_t is_tasks_executing = 0;

    void (*handler)(void *);
    uint8_t priority;
    void * arg;
    _interrupts_get_handler(&handler, &arg, &priority, trapframe);

    interrupt_tasks_add(handler, arg, priority);

    if (is_tasks_executing) {
        return;
    }

    is_tasks_executing = 1;

    struct trapframe nested_trapframe;
    trapframe_init(&nested_trapframe);
    nested_trapframe.sstatus = CSR_SSTATUS_SPIE | CSR_SSTATUS_SPP;
    csr_sscratch_set((uintptr_t)&nested_trapframe);

    interrupts_enable();

    while (!interrupt_tasks_is_empty()) {
        interrupt_tasks_execute();
    }

    interrupts_disable();
    csr_sscratch_set((uintptr_t)trapframe);

    is_tasks_executing = 0;

    if (_need_reschedule_cpu) {
        _need_reschedule_cpu = 0;
        cpu_scheduler_next();
    }
}

void _interrupts_external_handler(void * arg) {
    uint32_t irq = (uint32_t)arg;

    if (irq == uart_irq) {
        uart_irq_handler();
    }

    if (irq != 0) {
        plic_complete(irq);
    }
}
