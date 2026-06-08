#include "task_mapping.h"

#include "task.h"
#include "../mm/dynamic_allocator.h"
#include "../vmm/definitions.h"
#include "../vmm/virtual_memory.h"

#include "../mm/utils.h"

const uint64_t SIGNAL_TRAMPOLINE_VADDR = 0x1000;
const uint64_t USER_CODE_VADDR = 0x10000;
const uint64_t USER_STACK_VADDR = 0x003000000000;
const uint64_t SIGNAL_STACK_VADDR = 0x003800000000;

const uint64_t KERNEL_STACK_SIZE = 16384;
const uint64_t USER_STACK_SIZE = 16384;
const uint64_t SIGNAL_STACK_SIZE = 4096;

const uint32_t MAP_ANONYMOUS = 0x20;
const uint32_t MAP_POPULATE = 0x8000;

const uint32_t PROT_NONE = 0;
const uint32_t PROT_READ = 1;
const uint32_t PROT_WRITE = 2;
const uint32_t PROT_EXEC = 4;

extern void _signal_trampoline();

uint8_t get_mapping_prot(uint32_t user_prot) {
    uint8_t result = PTE_VALID | PTE_USER | PTE_ACCESSED | PTE_DIRTY;

    result |= ((user_prot & PROT_READ) ? PTE_READ : 0);
    result |= ((user_prot & PROT_WRITE) ? (PTE_WRITE | PTE_READ) : 0);
    result |= ((user_prot & PROT_EXEC) ? PTE_EXECUTE : 0);

    return result;
}

static uint64_t _get_mapping_vaddr(
    struct task * task, uint64_t vaddr, uint64_t size
) {
    uint64_t result = vaddr == 0 ? USER_CODE_VADDR : vaddr;

    if (result < SIGNAL_TRAMPOLINE_VADDR + pte_mem_size 
        && result + size > SIGNAL_TRAMPOLINE_VADDR
    ) {
        result = SIGNAL_TRAMPOLINE_VADDR + pte_mem_size;
    }

    struct mapping * current = (struct mapping *)task->mappings.head;

    while (current) {
        if (current->vaddr + current->size <= result) {
            current = (struct mapping *)current->node.next;
            continue;
        }

        if (current->vaddr >= result + size) {
            break;
        }

        result = current->vaddr + current->size;
        current = (struct mapping *)current->node.next;
    }

    if (size > KERNEL_VIRTUAL_BASE || result > KERNEL_VIRTUAL_BASE - size) {
        return (uint64_t)-1;
    }

    return result;
}

static void _insert_to_mapping(
    struct task * task, struct mapping * mapping
) {
    struct mapping * current = (struct mapping *)task->mappings.head;

    while (current) {
        if (current->vaddr > mapping->vaddr) {
            linked_list_insert_before(&task->mappings, &mapping->node, &current->node);
            return;
        }

        current = (struct mapping *)current->node.next;
    }

    linked_list_insert(&task->mappings, &mapping->node);
}

// TODO: support flags
struct mapping * task_add_mapping(
    struct task * task,
    uint64_t vaddr, uint64_t size, uint8_t prot,
    uint32_t flags
) {
    vaddr = align_to_pte(vaddr); 
    size = align_to_pte(size);
    uint64_t mapping_vaddr = _get_mapping_vaddr(task, vaddr, size);

    if (mapping_vaddr == (uint64_t)-1) {
        return 0;
    }

    // TODO: remove for demand paging
    uint64_t kernel_vaddr = (uint64_t)allocate(size);

    struct mapping * mapping = (struct mapping *)allocate(sizeof(struct mapping));
    mapping->node.next = mapping->node.prev = 0;
    mapping->vaddr = mapping_vaddr;
    mapping->kernel_vaddr = kernel_vaddr;
    mapping->size = size;
    mapping->prot = prot;

    _insert_to_mapping(task, mapping);

    // TODO: remove for demand paging
    virtual_memory_map(
        task->pgd,
        mapping_vaddr, va2pa(kernel_vaddr),
        size, prot
    );

    return mapping;
}

void task_create_user_mappings(struct task * task, void * code, uint64_t code_size) {
    // TODO result can be null
    struct mapping * code_mapping = task_add_mapping(
        task,
        USER_CODE_VADDR, code_size,
        PTE_USER_CODE_PROT, 0
    );

    // TODO: remove for demand paging
    memcopy((void *)code_mapping->kernel_vaddr, code, code_size);

    task_add_mapping(
        task,
        USER_STACK_VADDR, USER_STACK_SIZE,
        PTE_USER_STACK_PROT, 0
    );

    task_add_mapping(
        task,
        SIGNAL_STACK_VADDR, SIGNAL_STACK_SIZE,
        PTE_USER_STACK_PROT, 0
    );
    task->signal_sp = SIGNAL_STACK_VADDR + SIGNAL_STACK_SIZE;

    virtual_memory_map(
        task->pgd, SIGNAL_TRAMPOLINE_VADDR,
        va2pa((uint64_t)_signal_trampoline), pte_mem_size,
        PTE_USER_CODE_PROT
    );
}

void task_copy_mappings(struct task * dest, struct task * source) {
    struct mapping * current = (struct mapping *)source->mappings.head;

    while (current) {
        struct mapping * mapping = task_add_mapping(
            dest, 
            current->vaddr, current->size,
            current->prot, 0
        );

        memcopy((void *)mapping->kernel_vaddr, (void *)current->kernel_vaddr, current->size);

        current = (struct mapping *)current->node.next;
    }

    virtual_memory_map(
        dest->pgd, SIGNAL_TRAMPOLINE_VADDR,
        va2pa((uint64_t)_signal_trampoline), pte_mem_size,
        PTE_USER_CODE_PROT
    );
}

