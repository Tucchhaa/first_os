#include "syscalls.h"

#include "../task/task.h"
#include "../task/task_mapping.h"
#include "../task/task_signal.h"
#include "../task/task_table.h"
#include "../task/cpu_scheduler.h"
#include "../interrupts/interrupt_control.h"
#include "../mm/dynamic_allocator.h"
#include "../mm/utils.h"
#include "../../uart/uart.h"
#include "../task/kthreads.h"
#include "../initrd/initrd_parser.h"
#include "../../drivers/video/video.h"

#include "../../converters.h"

extern void switch_to_user();

static inline uint64_t _get_syscall_id(struct trapframe * tf) {
    return tf->regs[17];
}

static inline uint64_t _get_param_by_index(struct trapframe * tf, uint8_t index) {
    return tf->regs[10 + index];
}

static inline void _syscall_set_result(struct trapframe * tf, uint64_t value) {
    tf->regs[10] = value; // set a0 reg (return value)
} 

void _syscall_get_pid(struct trapframe * tf) {
    _syscall_set_result(tf, get_current_task()->pid);
}

void _syscall_uart_read(struct trapframe * tf) {
    char * buf = (char *)_get_param_by_index(tf, 0);
    uint32_t count = (uint32_t)_get_param_by_index(tf, 1);

    uint32_t read_count = uart_get_bytes(buf, count);

    if (read_count == 0) {
        tf->sepc -= 4;
        cpu_scheduler_wait(TASK_WAIT_UART_READ);
        return;
    }

    _syscall_set_result(tf, read_count);
}

void _syscall_uart_write(struct trapframe * tf) {
    const char * buf = (const char *)_get_param_by_index(tf, 0);
    uint64_t count = (uint64_t)_get_param_by_index(tf, 1);

    uint32_t write_count = uart_put_bytes(buf, count);

    if (write_count == 0) {
        tf->sepc -= 4;
        cpu_scheduler_wait(TASK_WAIT_UART_WRITE);
        return;
    }

    _syscall_set_result(tf, write_count);
}

// TODO: should replace task's memory
void _syscall_exec(struct trapframe * tf) {
    const char * path = (const char *)_get_param_by_index(tf, 0);

    struct task * new_task = task_create_user(path);
    if (new_task == 0) {
        _syscall_set_result(tf, -1);
        return;
    }

    kthread_exec_user(new_task);

    union task_wait_event_arg arg = { .i = new_task->pid };
    cpu_scheduler_wait_arg(TASK_WAIT_PROCESS_KILL, arg);

    _syscall_set_result(tf, 0);
}

void _syscall_fork(struct trapframe * tf) {
    struct task * current_task = get_current_task();
    struct task * child_task = task_copy(current_task);
    child_task->thread.ra = (uint64_t)switch_to_user;

    cpu_scheduler_add_task(child_task);

    struct trapframe * child_tf = (struct trapframe *)child_task->thread.sscratch;
    
    _syscall_set_result(child_tf, 0);
    _syscall_set_result(tf, child_task->pid);
}

// TODO: task with pid may have already been killed
void _syscall_waitpid(struct trapframe * tf) {
    struct task * current_task = get_current_task();
    uint32_t pid = (uint32_t)_get_param_by_index(tf, 0);

    union task_wait_event_arg arg = { .i = pid };
    cpu_scheduler_wait_arg(TASK_WAIT_PROCESS_KILL, arg);

    _syscall_set_result(tf, pid);
}

void _syscall_exit(struct trapframe * tf) {
    uint32_t status = (uint32_t)_get_param_by_index(tf, 0);

    cpu_scheduler_kill();

    _syscall_set_result(tf, 1);
}

void _syscall_stop(struct trapframe * tf) {
    uint32_t pid = _get_param_by_index(tf, 0);
    uint8_t result = cpu_scheduler_kill_by_pid(pid);

    _syscall_set_result(tf, result == 0 ? 0 : -1);
}

void _syscall_display(struct trapframe * tf) {
    uint32_t * bmp_image = (uint32_t *)_get_param_by_index(tf, 0);
    uint32_t width = (uint32_t)_get_param_by_index(tf, 1);
    uint32_t height = (uint32_t)_get_param_by_index(tf, 2);

    video_bmp_display(bmp_image, width, height);
}

void _syscall_usleep(struct trapframe * tf) {
    uint32_t usec = (uint32_t)_get_param_by_index(tf, 0);

    cpu_scheduler_sleep(usec);

    _syscall_set_result(tf, 0);
}

void _syscall_signal(struct trapframe * tf) {
    uint32_t signum = (uint32_t)_get_param_by_index(tf, 0);
    void (*handler)() = (void (*)())_get_param_by_index(tf, 1);
    
    task_register_signal(get_current_task(), signum, handler);

    _syscall_set_result(tf, 0);
}

void _syscall_sigreturn(struct trapframe * tf) {
    struct task * task = get_current_task();

    // Restore the saved trapframe from the signal stack
    memcopy(tf, (void *)task->signal_sp, sizeof(struct trapframe));
    task->signal_sp += sizeof(struct trapframe);
}

void _syscall_kill(struct trapframe * tf) {
    uint8_t pie = interrupts_disable();

    uint32_t result = 0;
    uint32_t pid = (uint32_t)_get_param_by_index(tf, 0);
    uint32_t signum = (uint32_t)_get_param_by_index(tf, 1);

    struct task * task = task_table_get_task(pid);

    if (task == 0) {
        result = 1;
    } 
    else {
        struct signal * signal = task_get_signal(task, signum);

        if (signal == 0) {
            result = cpu_scheduler_kill_by_pid(pid);
        } else {
            signal->signum = signum;
            signal->is_pending = 1;
        }
    } 

    interrupts_restore(pie);
    _syscall_set_result(tf, result);
}

void _syscall_mmap(struct trapframe * tf) {
    uintptr_t vaddr = (uintptr_t)_get_param_by_index(tf, 0);
    uintptr_t size = (uintptr_t)_get_param_by_index(tf, 1);
    uint32_t user_prot = (uint32_t)_get_param_by_index(tf, 2);
    uint32_t flags = (uint32_t)_get_param_by_index(tf, 3);

    // Note: if RWX is not set to PTE, MMU will treat it as non-leaf entry
    if (size == 0 || user_prot == PROT_NONE) {
        _syscall_set_result(tf, (uint64_t)-1);
        return;
    }

    uint8_t pie = interrupts_disable();
    struct mapping * mapping = task_add_mapping(
        get_current_task(),
        vaddr, size, 
        get_mapping_prot(user_prot), flags
    );

    if (mapping == 0) {
        _syscall_set_result(tf, (uint64_t)-1);
        return;
    }

    interrupts_restore(pie);
    
    _syscall_set_result(tf, mapping->vaddr);
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
    case 8: return _syscall_display(tf);
    case 9: return _syscall_usleep(tf);
    case 10: return _syscall_signal(tf);
    case 11: return _syscall_sigreturn(tf);
    case 12: return _syscall_kill(tf);
    case 13: return _syscall_mmap(tf);
    default:
        return;
    }
}
