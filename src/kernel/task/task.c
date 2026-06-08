#include "task.h"

#include "../mm/dynamic_allocator.h"
#include "../mm/utils.h"
#include "../vmm/definitions.h"
#include "../vmm/virtual_memory.h"
#include "../interrupts/interrupt_control.h"
#include "../initrd/initrd.h"
#include "task_mapping.h"
#include "task_signal.h"

static uint32_t get_next_pid() {
    uint8_t pie = interrupts_disable();
    static uint32_t next_pid = 1;
    uint32_t result = next_pid++;
    interrupts_restore(pie);
    return result;
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

    for (uint32_t i=0; i < 32; i++) {
        trapframe->regs[i] = 0;
    }

    trapframe->sepc = 0;
    trapframe->sstatus = 0;

    // Init task.thread
    task->thread.ra = 0;
    task->thread.sp = task->kernel_stack_top;
    task->thread.sscratch = (uint64_t)trapframe;
    task->thread.sstatus = 0;
    task->thread.satp = get_satp_value(va2pa((uint64_t)kernel_pgd));

    return task;
}

struct task * task_create_user(const char * filepath) {
    uintptr_t file_addr = initrd_get_file_addr(filepath);

    if (file_addr == 0) {
        return 0;
    }

    struct task * task = task_create();

    // PGD
    {
    task->is_user = 1;
    task->pgd = (uint64_t *)allocate(sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
    task->thread.satp = get_satp_value(va2pa((uint64_t)task->pgd));
    memzero(task->pgd, sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);

    for (uint32_t i = PAGE_TABLE_ENTRIES_NUM / 2; i < PAGE_TABLE_ENTRIES_NUM; i += 1) {
        task->pgd[i] = kernel_pgd[i];
    }
    }

    // Create mappings
    {
    uintptr_t data;
    uint32_t data_size;
    initrd_get_filedata(file_addr, &data, &data_size);

    task_create_user_mappings(task, (void *)data, data_size);
    }

    // Trapframe
    {
    uint64_t user_sp = USER_STACK_VADDR + USER_STACK_SIZE;

    struct trapframe * trapframe = (struct trapframe *)task->thread.sscratch;
    trapframe->sepc = USER_CODE_VADDR;
    trapframe->regs[2] = user_sp; // set sp reg
    // TODO: user process should not have access to struct task
    trapframe->regs[4] = (uint64_t)task; // set tp reg
    // TODO: set ra reg, to make exit syscall
    }

    return task;
}

static inline uintptr_t _rebase_kstack_addr(
    struct task * task,
    struct task * source,
    uintptr_t source_addr
) {
    return task->kernel_stack_addr + (source_addr - source->kernel_stack_addr);
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

    if (task->pgd) {
        free((void *)task->pgd);
    }

    struct mapping * mapping = (struct mapping *)task->mappings.head;

    while(mapping) {
        struct mapping * next = (struct mapping *)mapping->node.next; 
        free((void *)mapping->kernel_vaddr);
        free(mapping);
        mapping = next;
    }

    // TODO: free task->pgd tables
    if (task->is_user) {
        struct signal * signal = (struct signal *)task->signals_list.head;

        while (signal) {
            struct signal * next = (struct signal *)signal->node.next;
            free(signal);
            signal = next;
        }
    }

    free((void *)task);
}
