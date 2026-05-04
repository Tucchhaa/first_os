#include "syscalls.h"

#include "../task/task.h"
#include "../task/cpu_scheduler.h"
#include "../mm/dynamic_allocator.h"
#include "../../uart/uart.h"
#include "interrupts.h"

#include "../../converters.h"

static inline uint64_t _get_syscall_id(struct trapframe * tf) {
    return tf->regs[17];
}

static inline uint64_t _get_param_by_index(struct trapframe * tf, uint8_t index) {
    return tf->regs[10 + index];
}

static inline void _syscall_set_result(struct trapframe * tf, uint64_t value) {
    tf->regs[10] = value;
} 

void _syscall_get_pid(struct trapframe * tf) {
    _syscall_set_result(tf, get_current_task()->pid);
}

void _syscall_uart_read(struct trapframe * tf) {
    char * buf = (char *)_get_param_by_index(tf, 0);
    uint32_t count = (uint32_t)_get_param_by_index(tf, 1);

    if (uart_receive_buf_empty()) {
        tf->sepc -= 4;
        cpu_scheduler_wait(WAIT_UART_READ);
        set_need_reschedule_cpu(0);
        return;
    }

    uint32_t read_count = uart_get_bytes(buf, count);
    _syscall_set_result(tf, read_count);
}

void _syscall_uart_write(struct trapframe * tf) {
    const char * buf = (const char *)_get_param_by_index(tf, 0);
    uint64_t count = (uint64_t)_get_param_by_index(tf, 1);

    if (uart_transmit_buf_full()) {
        tf->sepc -= 4;
        cpu_scheduler_wait(WAIT_UART_WRITE);
        set_need_reschedule_cpu(0);
        return;
    }
    
    uint32_t write_count = uart_put_bytes(buf, count);
    _syscall_set_result(tf, write_count);
}

void _syscall_exec(struct trapframe * tf) {

}

void _syscall_fork(struct trapframe * tf) {
    struct task * current_task = get_current_task();
    struct task * child_task = task_copy(current_task);

    cpu_scheduler_add_task(child_task);
    
    struct trapframe * child_tf = (struct trapframe *)child_task->thread.sscratch;
    child_tf->regs[10] = 0;

    _syscall_set_result(tf, child_task->pid);
}

// TODO: task with pid may already be killed
void _syscall_waitpid(struct trapframe * tf) {
    struct task * current_task = get_current_task();
    uint32_t pid = (uint32_t)_get_param_by_index(tf, 0);
    
    union task_wait_event_arg arg = { .i = pid };
    cpu_scheduler_wait_arg(WAIT_PROCESS_KILL, arg);
    set_need_reschedule_cpu(0);

    _syscall_set_result(tf, pid);
}

void _syscall_exit(struct trapframe * tf) {
    uint32_t status = (uint32_t)_get_param_by_index(tf, 0);

    cpu_scheduler_kill();
    set_need_reschedule_cpu(0);
}

void _syscall_stop(struct trapframe * tf) {

}

void syscall_handler(void * arg) {
    struct trapframe * tf = (struct trapframe *)arg;
    tf->sepc += 4;

    switch (_get_syscall_id(tf))
    {
    case 0: return _syscall_get_pid(tf);
    case 1: return _syscall_uart_read(tf);
    case 2: return _syscall_uart_write(tf);
    case 3: return _syscall_exec(tf);
    case 4: return _syscall_fork(tf);
    case 5: return _syscall_waitpid(tf);
    case 6: return _syscall_exit(tf);
    case 7: return _syscall_stop(tf);
    default:
        return;
    }
}
