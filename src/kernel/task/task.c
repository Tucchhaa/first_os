#include "task.h"

#include "../mm/dynamic_allocator.h"
#include "../mm/utils.h"
#include "../vmm/definitions.h"
#include "../vmm/virtual_memory.h"
#include "../interrupts/interrupt_control.h"
#include "../initrd/initrd.h"

const uint64_t KERNEL_STACK_SIZE = 16384;
const uint64_t USER_STACK_SIZE = 16384;
const uint64_t SIGNAL_STACK_SIZE = 4096;

const uint64_t USER_PROGRAM_VADDR = 0x10000;
const uint64_t USER_STACK_VADDR = 0x003000000000;
const uint64_t SIGNAL_STACK_VADDR = 0x003800000000;

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
    task->pgd = 0;
    task->state = TASK_STATE_READY;
    task->wait_event.id = TASK_WAIT_NONE;

    task->kernel_stack_addr = (uintptr_t)allocate(KERNEL_STACK_SIZE);
    task->kernel_sp = task->kernel_stack_addr + KERNEL_STACK_SIZE;

    // Init task.trapframe
    task->kernel_sp -= sizeof(struct trapframe);
    struct trapframe * trapframe = (struct trapframe *)task->kernel_sp;

    for (uint32_t i=0; i < 32; i++) {
        trapframe->regs[i] = 0;
    }

    trapframe->sepc = 0;
    trapframe->sstatus = 0;

    // Init task.thread
    task->thread.ra = 0;
    task->thread.sp = task->kernel_sp;
    task->thread.sscratch = (uint64_t)trapframe;
    task->thread.sstatus = 0;
    task->thread.satp = get_satp_value(va2pa((uint64_t)kernel_pgd));

    return task;
}

static void _task_add_user_mapping(
    struct task * task,
    uint64_t vaddr, uint64_t kernel_vaddr,
    uint64_t size, uint8_t prot
) {
    struct user_mapping * mapping = (struct user_mapping *)allocate(sizeof(struct user_mapping));
    mapping->node.next = mapping->node.prev = 0;
    mapping->vaddr = vaddr;
    mapping->kernel_vaddr = kernel_vaddr;
    mapping->size = size;
    mapping->prot = prot;

    linked_list_insert(&task->user_mappings, &mapping->node);

    virtual_memory_map(
        task->pgd,
        vaddr, va2pa(kernel_vaddr),
        size, prot
    );
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

    void * user_program = allocate(data_size);
    memcopy(user_program, (void *)data, data_size);

    task->signal_sp = SIGNAL_STACK_VADDR + SIGNAL_STACK_SIZE;
    
    linked_list_init(&task->signals_list);
    linked_list_init(&task->user_mappings);

    _task_add_user_mapping(
        task, 
        USER_PROGRAM_VADDR, (uint64_t)user_program,
        data_size, PTE_USER_CODE_PROT
    );

    _task_add_user_mapping(
        task, 
        USER_STACK_VADDR, (uint64_t)allocate(USER_STACK_SIZE),
        USER_STACK_SIZE, PTE_USER_STACK_PROT
    );

    _task_add_user_mapping(
        task, 
        SIGNAL_STACK_VADDR, (uint64_t)allocate(SIGNAL_STACK_SIZE),
        SIGNAL_STACK_SIZE, PTE_USER_STACK_PROT
    );
    }

    // Trapframe
    {
    uint64_t user_sp = USER_STACK_VADDR + USER_STACK_SIZE;

    struct trapframe * trapframe = (struct trapframe *)task->thread.sscratch;
    trapframe->sepc = USER_PROGRAM_VADDR;
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

struct task * task_copy(struct task * source) {
    struct task * task = allocate(sizeof(struct task));
    memzero(task, sizeof(struct task));

    task->pid = get_next_pid();
    task->state = TASK_STATE_READY;
    task->is_user = source->is_user;

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
    task->kernel_sp = _rebase_kstack_addr(task, source, source->kernel_sp);
    memcopy((void *)task->kernel_stack_addr, (void *)source->kernel_stack_addr, KERNEL_STACK_SIZE);
    }

    // User mappings
    {
    linked_list_init(&task->user_mappings);

    struct user_mapping * src_mapping = (struct user_mapping *)source->user_mappings.head;

    while (src_mapping) {
        void * new_pages = allocate(src_mapping->size);
        memcopy(new_pages, (void *)src_mapping->kernel_vaddr, src_mapping->size);

        _task_add_user_mapping(
            task,
            src_mapping->vaddr, (uint64_t)new_pages,
            src_mapping->size, src_mapping->prot
        );

        src_mapping = (struct user_mapping *)src_mapping->node.next;
    }

    task->signal_sp = source->signal_sp;
    }

    // Signals
    {
    linked_list_init(&task->signals_list);

    struct signal * signal = (struct signal *)source->signals_list.head;

    while (signal) {
        struct signal * signal_copy = (struct signal *)allocate(sizeof(struct signal));
        signal_copy->signum = signal->signum;
        signal_copy->handler = signal->handler;
        signal_copy->is_pending = signal->is_pending;
        linked_list_insert(&task->signals_list, &signal_copy->node);

        signal = (struct signal *)signal->node.next;
    }
    }

    // Copy task.thread
    {
    task->thread.ra = source->thread.ra;
    task->thread.sp = task->kernel_sp;
    task->thread.sscratch = _rebase_kstack_addr(task, source, source->thread.sscratch);
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

    struct user_mapping * mapping = (struct user_mapping *)task->user_mappings.head;

    while(mapping) {
        struct user_mapping * next = (struct user_mapping *)mapping->node.next; 
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

void task_register_signal(
    struct task * task, uint32_t signum, void (*handler)()
) {
    struct signal * signal = (struct signal *)allocate(sizeof(struct signal));

    signal->signum = signum;
    signal->handler = handler;
    signal->is_pending = 0;

    linked_list_insert(&task->signals_list, &signal->node);
}

struct signal * task_get_signal(struct task * task, uint32_t signum) {
    if (!task->is_user) {
        return 0;
    }

    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        if (signal->signum == signum) {
            return signal;
        }

        signal = (struct signal *)signal->node.next;
    }

    return 0;
}

struct signal * task_get_next_pending_signal(struct task * task) {
    if (!task->is_user) {
        return 0;
    }

    struct signal * signal = (struct signal *)task->signals_list.head;

    while (signal) {
        if (signal->is_pending) {
            return signal;
        }

        signal = (struct signal *)signal->node.next;
    }

    return 0;
}
