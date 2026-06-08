#pragma once

#include <stdint.h>

#include "../ds/linked_list.h"

struct task;

extern const uint64_t SIGNAL_TRAMPOLINE_VADDR;
extern const uint64_t USER_CODE_VADDR;
extern const uint64_t USER_STACK_VADDR;
extern const uint64_t SIGNAL_STACK_VADDR;

extern const uint64_t KERNEL_STACK_SIZE;
extern const uint64_t USER_STACK_SIZE;
extern const uint64_t SIGNAL_STACK_SIZE;

extern const uint32_t PROT_NONE;
extern const uint32_t PROT_READ;
extern const uint32_t PROT_WRITE;
extern const uint32_t PROT_EXEC;

struct mapping {
    struct linked_list_node node;
    uint64_t vaddr;
    uint64_t kernel_vaddr;
    uint64_t size;
    uint8_t prot;
};

uint8_t get_mapping_prot(uint32_t user_prot);

struct mapping * task_add_mapping(
    struct task * task,
    uint64_t vaddr, uint64_t size, uint8_t prot,
    uint32_t flags
);

void task_create_user_mappings(struct task * task, void * code, uint64_t code_size);

void task_copy_mappings(struct task * dest, struct task * source);

void task_free_mappings(struct task * task);
