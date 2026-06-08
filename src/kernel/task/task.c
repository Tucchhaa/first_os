#include "task.h"

#include "../mm/dynamic_allocator.h"
#include "../mm/utils.h"
#include "../vmm/definitions.h"
#include "../vmm/virtual_memory.h"
#include "../interrupts/interrupt_control.h"
#include "../initrd/initrd.h"
#include "../interrupts/csr.h"
#include "task_mapping.h"
#include "task_signal.h"

extern void switch_to_user();

static uint32_t get_next_pid() {
    uint8_t pie = interrupts_disable();
    static uint32_t next_pid = 1;
    uint32_t result = next_pid++;
    interrupts_restore(pie);
    return result;
}

static void _task_init_thread(struct task * task, uint64_t trapframe_addr, uint64_t pgd) {
    for (uint32_t i=0; i < 12; i += 1) {
        task->thread.s[i] = 0;
    }

    task->thread.ra = 0;
    task->thread.sp = task->kernel_stack_top;
    task->thread.sscratch = trapframe_addr;
    task->thread.sstatus = CSR_SSTATUS_SIE | CSR_SSTATUS_SPP;
    task->thread.satp = get_satp_value(va2pa(pgd));
}

static void _task_init_trapframe(struct trapframe * trapframe) {
    for (uint32_t i=0; i < 32; i++) {
        trapframe->regs[i] = 0;
    }

    trapframe->sepc = 0;
    trapframe->sstatus = 0;
}

struct task * task_create() {
    struct task * task = allocate(sizeof(struct task));
    memzero((void *)task, sizeof(struct task));
    
    task->pid = get_next_pid();
    task->is_user = 0;
    task->pgd = 0;
    task->state = TASK_STATE_READY;
    task->wait_event.id = TASK_WAIT_NONE;

    linked_list_init(&task->mappings);
    linked_list_init(&task->signals_list);

    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_stack_top = task->kernel_stack_addr + KERNEL_STACK_SIZE;

    // Init task.trapframe
    task->kernel_stack_top -= sizeof(struct trapframe);
    struct trapframe * trapframe = (struct trapframe *)task->kernel_stack_top;

    _task_init_trapframe(trapframe);
    _task_init_thread(task, (uint64_t)trapframe, (uint64_t)kernel_pgd);

    return task;
}

uint8_t task_exec_user(struct task * task, const char * filepath) {
    uintptr_t file_addr = initrd_get_file_addr(filepath);

    if (file_addr == 0) {
        return 1;
    }

    task->is_user = 1;
    task->state = TASK_STATE_READY;
    task->wait_event.id = TASK_WAIT_NONE;
    task_free_mappings(task);
    task_free_signals(task);
    
    linked_list_init(&task->mappings);
    linked_list_init(&task->signals_list);

    // PGD
    {
    if (task->pgd == 0) {
        task->pgd = (uint64_t *)allocate(sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);

        for (uint32_t i = PAGE_TABLE_ENTRIES_NUM / 2; i < PAGE_TABLE_ENTRIES_NUM; i += 1) {
            task->pgd[i] = kernel_pgd[i];
        }
    } else {
        virtual_memory_free_tables(task->pgd, 0, PAGE_TABLE_ENTRIES_NUM / 2);
    }
    }

    // Mappings
    {
    uintptr_t code;
    uint32_t code_size;
    initrd_get_filedata(file_addr, &code, &code_size);

    task_create_user_mappings(task, (void *)code, code_size);
    }

    // Thread & Trapframe
    {
    struct trapframe * trapframe = (struct trapframe *)task->kernel_stack_top;

    _task_init_trapframe(trapframe);
    _task_init_thread(task, (uint64_t)trapframe, (uint64_t)task->pgd);

    task->thread.ra = (uint64_t)switch_to_user;
    task->thread.sstatus |= CSR_SSTATUS_SUM;

    trapframe->regs[2] = USER_STACK_VADDR + USER_STACK_SIZE; // set sp reg
    // TODO: user process should not have access to struct task
    trapframe->regs[4] = (uint64_t)task; // set tp reg
    // TODO: set ra reg, to make exit syscall

    trapframe->sepc = USER_CODE_VADDR;
    trapframe->sstatus = CSR_SSTATUS_SPIE | CSR_SSTATUS_SUM;
    }

    return 0;
}

// TODO: doesn't work with kernel tasks
struct task * task_copy(struct task * source) {
    struct task * task = allocate(sizeof(struct task));
    memzero(task, sizeof(struct task));

    task->pid = get_next_pid();
    task->state = TASK_STATE_READY;
    task->is_user = source->is_user;

    linked_list_init(&task->mappings);
    linked_list_init(&task->signals_list);

    // PGD
    {
    task->pgd = (uint64_t *)allocate(sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
    memzero(task->pgd, sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
    
    for (uint32_t i = PAGE_TABLE_ENTRIES_NUM / 2; i < PAGE_TABLE_ENTRIES_NUM; i++) {
        task->pgd[i] = kernel_pgd[i];
    }
    }

    // Kernel stack
    {
    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_stack_top = task->kernel_stack_addr + KERNEL_STACK_SIZE - sizeof(struct trapframe);

    memcopy(
        (void *)task->kernel_stack_top, 
        (void *)source->kernel_stack_top,
        sizeof(struct trapframe)
    );
    }

    task_copy_mappings(task, source);
    task_copy_signals(task, source);

    // Copy task.thread
    {
    // TODO: rewritten by fork()
    task->thread.ra = source->thread.ra;
    task->thread.sp = task->kernel_stack_top;
    task->thread.sscratch = task->kernel_stack_top;
    task->thread.sstatus = source->thread.sstatus;
    task->thread.satp = get_satp_value(va2pa((uint64_t)task->pgd));

    for (uint32_t i=0; i < 12; i++) {
        task->thread.s[i] = source->thread.s[i];
    }
    }

    // Update trapframe addresses
    struct trapframe * trapframe = (struct trapframe *)task->thread.sscratch;
    trapframe->regs[4] = (uint64_t)task; // set tp reg

    return task;
}

void task_free(struct task * task) {
    free((void *)task->kernel_stack_addr);

    task_free_mappings(task);
    task_free_signals(task);

    if (task->pgd) {
        virtual_memory_free_tables(task->pgd, 0, PAGE_TABLE_ENTRIES_NUM / 2);
        free(task->pgd);
    }

    free((void *)task);
}
