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

extern const uint32_t MAP_ANONYMOUS;
extern const uint32_t MAP_FILE;
extern const uint32_t MAP_POPULATE;

extern const uint32_t PROT_NONE;
extern const uint32_t PROT_READ;
extern const uint32_t PROT_WRITE;
extern const uint32_t PROT_EXEC;

extern const uint8_t BACKER_ANON;
extern const uint8_t BACKER_FILE;
extern const uint8_t BACKER_PARENT_COPY;

struct mapping {
    struct linked_list_node node;
    uint64_t vaddr;
    uint64_t size;
    uint8_t prot;
    uint8_t backer;
    void * file_data;
    uint64_t file_size;
};

uint8_t get_mapping_prot(uint32_t user_prot);

struct mapping * task_add_mapping(
    struct task * task,
    uint64_t vaddr, uint64_t size, uint8_t prot,
    uint32_t flags, void * file_data
);

void task_create_user_mappings(struct task * task, void * code, uint64_t code_size);

void task_copy_mappings(struct task * dest, struct task * source);

struct mapping * task_find_mapping(struct task * task, uint64_t vaddr);

void task_install_page(struct task * task, struct mapping * mapping, uint64_t vaddr);

void task_free_mappings(struct task * task);
