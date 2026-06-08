#include "interrupt_handler.h"

#include <stdint.h>

#include "csr.h"
#include "plic.h"
#include "timeouts.h"
#include "syscalls.h"
#include "interrupt_control.h"
#include "interrupt_tasks.h"
#include "../task/task.h"
#include "../task/task_signal.h"
#include "../task/task_mapping.h"
#include "../task/cpu_scheduler.h"
#include "../../uart/uart_sync.h"
#include "../../uart/uart.h"
#include "../mm/utils.h"

#include "../../converters.h"

uint8_t is_handling_interrupt = 0;

static uint8_t _need_reschedule_cpu = 0;

void set_need_reschedule_cpu() { 
    _need_reschedule_cpu = 1; 
}

extern void _interrupt_entry();

void interrupt_setup() {
    uart_sync_puts("[KERNEL:INTERRUPTS] Setting up...\n");

    interrupt_tasks_setup();
    timeouts_setup();
    csr_stvec_set((uintptr_t)_interrupt_entry);

    uart_sync_puts("[KERNEL:INTERRUPTS] Done setting up\n");
}

static void _interrupt_external_handler(void * arg);

static void _signal_handler(struct trapframe * trapframe);

static uint8_t _convert_plic_priority(uint8_t plic_priority) {
    const uint8_t PLIC_MAX_PRIORITY = 7;
    const uint8_t HANDLER_MAX_PRIORITY = 255;

    return HANDLER_MAX_PRIORITY - (plic_priority * HANDLER_MAX_PRIORITY / PLIC_MAX_PRIORITY);
}

static void _default_fault_handler(void *) {
    cpu_scheduler_kill();
}

static void _interrupt_get_handler(
    void (**handler)(void *),
    void ** arg,
    uint8_t * priority,
    struct trapframe * trapframe
) {
    uint64_t scause = csr_scause_get();

    *priority = 1;
    *arg = (void *)0;

    switch (scause)
    {
    case ((1ULL << 63) | 5): // is_timer
        timeouts_postpone();
        *priority = 200;
        *handler = &timeouts_interrupt_handler;
        break;
    case ((1ULL << 63) | 9): // is_external
        *arg = (void *)((uint64_t)plic_claim());
        *handler = &_interrupt_external_handler;
        break;
    case 8: // U-mode ecall
        *arg = trapframe;
        *handler = &syscall_handler;
        break;
    case 0xd: // Segmentation fault
        uart_debug_puts("[SEGMENTATION FAULT]: kill process\n");
        *handler = &_default_fault_handler;
        break;
    case 0xf:
        uart_debug_puts("[PERMISSIONS FAULT]: kill process\n");
        *handler = &_default_fault_handler;
        break;
    default:
        uart_debug_puts("KERNEL PANIC!!!\n");
        while (1) {}
        break;
    }
}

void interrupt_handler(struct trapframe * trapframe) {
    void (*handler)(void *);
    uint8_t priority;
    void * arg;
    _interrupt_get_handler(&handler, &arg, &priority, trapframe);

    interrupt_tasks_add(handler, arg, priority);

    if (is_handling_interrupt) {
        return;
    }

    is_handling_interrupt = 1;

    struct trapframe nested_trapframe;
    csr_sscratch_set((uintptr_t)&nested_trapframe);

    interrupts_enable();
    while (interrupt_tasks_execute()) { }
    interrupts_disable();

    csr_sscratch_set((uintptr_t)trapframe);

    is_handling_interrupt = 0;

    if (_need_reschedule_cpu) {
        _need_reschedule_cpu = 0;
        cpu_scheduler_next();
    }

    _signal_handler(trapframe);
}

static void _interrupt_external_handler(void * arg) {
    uint32_t irq = (uint32_t)((uint64_t)arg);

    if (irq == uart_irq) {
        uart_irq_handler();
    }

    if (irq != 0) {
        plic_complete(irq);
    }
}

static void _signal_handler(struct trapframe * trapframe) {
    struct task * task = get_current_task();
    struct signal * signal = task_get_next_pending_signal(task);

    if (signal) {
        signal->is_pending = 0;

        task->signal_sp -= sizeof(struct trapframe);
        memcopy((void *)task->signal_sp, trapframe, sizeof(struct trapframe));

        trapframe->sepc = (uint64_t)signal->handler;
        trapframe->regs[1] = SIGNAL_TRAMPOLINE_VADDR; // set ra reg
        trapframe->regs[2] = task->signal_sp; // set sp reg
        trapframe->regs[10] = signal->signum;
    }
}
