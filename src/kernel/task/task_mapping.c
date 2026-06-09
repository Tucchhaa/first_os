#include "task_mapping.h"

#include "task.h"
#include "../mm/dynamic_allocator.h"
#include "../vmm/definitions.h"
#include "../vmm/virtual_memory.h"

#include "../mm/utils.h"
#include "../mm/page_allocator.h"

const uint64_t SIGNAL_TRAMPOLINE_VADDR = 0x1000;
const uint64_t USER_CODE_VADDR = 0x10000;
const uint64_t USER_STACK_VADDR = 0x003000000000;
const uint64_t SIGNAL_STACK_VADDR = 0x003800000000;

const uint64_t KERNEL_STACK_SIZE = 16384;
const uint64_t USER_STACK_SIZE = 16384;
const uint64_t SIGNAL_STACK_SIZE = 4096;

const uint32_t MAP_ANONYMOUS = 0x20;
const uint32_t MAP_FILE = 0x1;
const uint32_t MAP_POPULATE = 0x8000;

const uint32_t PROT_NONE = 0;
const uint32_t PROT_READ = 1;
const uint32_t PROT_WRITE = 2;
const uint32_t PROT_EXEC = 4;

const uint8_t BACKER_ANON = 1;
const uint8_t BACKER_FILE = 2;
const uint8_t BACKER_PARENT_COPY = 3;

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

static void _insert_mapping(
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

struct mapping * task_add_mapping(
    struct task * task,
    uint64_t vaddr, uint64_t size, 
    uint8_t prot, uint32_t flags, void * file_data
) {
    if ((flags & MAP_FILE) && (flags & MAP_ANONYMOUS)) {
        return 0;
    }

    uint64_t aligned_vaddr = align_to_pte(vaddr); 
    uint64_t aligned_size = align_to_pte(size);

    uint64_t mapping_vaddr = _get_mapping_vaddr(task, aligned_vaddr, aligned_size);

    if (mapping_vaddr == (uint64_t)-1) {
        return 0;
    }

    struct mapping * mapping = (struct mapping *)allocate(sizeof(struct mapping));
    mapping->node.next = mapping->node.prev = 0;
    mapping->vaddr = mapping_vaddr;
    mapping->size = aligned_size;
    mapping->prot = prot;
    mapping->file_data = 0;
    mapping->backer = BACKER_ANON;

    _insert_mapping(task, mapping);

    if (flags & MAP_FILE) {
        mapping->backer = BACKER_FILE;
        mapping->file_data = file_data;
        mapping->file_size = size;
    }

    if (flags & MAP_POPULATE) {
        for (uint64_t offset = 0; offset < aligned_size; offset += pte_mem_size) {
            task_install_page(task, mapping, mapping_vaddr + offset);
        }
    }

    return mapping;
}

void task_create_user_mappings(struct task * task, void * file_data, uint64_t file_size) {
    // TODO result can be null
    struct mapping * code_mapping = task_add_mapping(
        task,
        USER_CODE_VADDR, file_size,
        PTE_USER_CODE_PROT, MAP_FILE, file_data
    );

    task_add_mapping(
        task,
        USER_STACK_VADDR, USER_STACK_SIZE,
        PTE_USER_ANON_PROT, MAP_ANONYMOUS, 0
    );

    task_add_mapping(
        task,
        SIGNAL_STACK_VADDR, SIGNAL_STACK_SIZE,
        PTE_USER_ANON_PROT, MAP_ANONYMOUS, 0
    );
    task->signal_sp = SIGNAL_STACK_VADDR + SIGNAL_STACK_SIZE;

    // TODO
    virtual_memory_map(
        task->pgd, SIGNAL_TRAMPOLINE_VADDR,
        va2pa((uint64_t)_signal_trampoline), pte_mem_size,
        PTE_USER_CODE_PROT
    );
}

struct mapping_ctx {
    struct mapping * mapping;
    uint64_t * pgd;
};

static void _copy_one_page(uint64_t * pte, uint64_t vaddr, void * _ctx) {
    struct mapping_ctx * ctx = (struct mapping_ctx *)_ctx;

    void * src_page = vaddr_from_pte(*pte);

    void * dest_page = memory_allocate_pages(pte_mem_size);
    memcopy(dest_page, src_page, pte_mem_size);

    virtual_memory_map(
        ctx->pgd,
        vaddr, va2pa((uint64_t)dest_page),
        pte_mem_size, ctx->mapping->prot
    );
}

static void _free_one_page(uint64_t * pte, uint64_t vaddr, void * ctx) {
    void * src_page = vaddr_from_pte(*pte);
    memory_free_pages(src_page);
}

void task_copy_mappings(struct task * dest, struct task * source) {
    struct mapping * current = (struct mapping *)source->mappings.head;

    while (current) {
        uint32_t flags = (current->backer == BACKER_FILE ? MAP_FILE : MAP_ANONYMOUS);

        struct mapping * mapping = task_add_mapping(
            dest, 
            current->vaddr, current->size,
            current->prot, flags, current->file_data
        );

        if (mapping == 0 || mapping->vaddr != current->vaddr) {
            // TODO: not supported
            return;
        }

        struct mapping_ctx ctx = {
            .mapping = mapping,
            .pgd = dest->pgd
        };

        // TODO: remove when CoW is implemented
        virtual_memory_traverse_leafs(
            source->pgd, 2, 0,
            current->vaddr, current->vaddr + current->size,
            _copy_one_page, (void *)&ctx
        );

        current = (struct mapping *)current->node.next;
    }

    virtual_memory_map(
        dest->pgd, SIGNAL_TRAMPOLINE_VADDR,
        va2pa((uint64_t)_signal_trampoline), pte_mem_size,
        PTE_USER_CODE_PROT
    );
}

struct mapping * task_find_mapping(struct task * task, uint64_t vaddr) {
    struct mapping * current = (struct mapping *)task->mappings.head;

    while (current) {
        if (vaddr >= current->vaddr && vaddr < current->vaddr + current->size) {
            return current;
        }

        if (current->vaddr > vaddr) {
            return 0;
        }

        current = (struct mapping *)current->node.next;
    }

    return 0;
}

void task_install_page(struct task * task, struct mapping * mapping, uint64_t vaddr) {
    uint64_t kernel_vaddr = (uint64_t)memory_allocate_pages(pte_mem_size);
    memzero((void *)kernel_vaddr, pte_mem_size);

    if (mapping->backer == BACKER_FILE) {
        uint64_t offset = vaddr - mapping->vaddr;

        if (offset < mapping->file_size) {
            uint64_t n = offset + pte_mem_size >= mapping->file_size 
                ? mapping->file_size - offset
                : pte_mem_size;

            void * src = (void *)((uint64_t)mapping->file_data + offset);

            memcopy((void *)kernel_vaddr, src, n);
        }
    }

    virtual_memory_map(
        task->pgd,
        vaddr, va2pa(kernel_vaddr),
        pte_mem_size, mapping->prot
    );
}

void task_free_mappings(struct task * task) {
    struct mapping * current = (struct mapping *)task->mappings.head;

    while(current) {
        struct mapping * next = (struct mapping *)current->node.next; 

        virtual_memory_traverse_leafs(
            task->pgd, 2, 0,
            current->vaddr, current->vaddr + current->size,
            _free_one_page, 0
        );

        free(current);
        current = next;
    }
}
