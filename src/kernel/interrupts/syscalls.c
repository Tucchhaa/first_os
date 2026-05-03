#include "syscalls.h"

#include "../task/task.h"
#include "../../uart/uart.h"

#include "../../converters.h"

static struct trapframe * tf;

static inline uint64_t _get_syscall_id() {
    return tf->regs[17];
}

static inline uint64_t _get_param_by_index(uint8_t index) {
    return tf->regs[10 + index];
}

static inline void _set_syscall_result(uint64_t value) {
    tf->regs[10] = value;
} 

void _syscall_get_pid() {
    _set_syscall_result(get_current_task()->pid);
}

void _syscall_uart_read() {
    
}

void syscall_handler(void * arg) {
    tf = (struct trapframe *)arg;
    // char c[40];
    // itoa(get_current_task()->pid, c);
    // uart_puts_variadic("syscall, pid: ", c, "\n", 0);

    // tf = trapframe;

    // switch (_get_syscall_id())
    // {
    // case 0: return _syscall_get_pid();
    // case 1: return _syscall_uart_read();
    // default:
    //     return;
    // }
}
