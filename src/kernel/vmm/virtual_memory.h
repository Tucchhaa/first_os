#pragma once

#include <stdint.h>

extern uint64_t kernel_pgd[512];
extern uint8_t virtual_memory_ready;

void set_virtual_kernel_offset(uint64_t value);

uint64_t get_satp_value(uint64_t pgd_paddr);

uint64_t va2pa(uint64_t virtual_addr);
uint64_t pa2va(uint64_t physical_addr);

uint64_t * vaddr_from_pte(uint64_t pte);

void virtual_memory_map(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr,
    uint64_t size, 
    uint64_t prot
);

void virtual_memory_map_pmd(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr,
    uint64_t size, 
    uint64_t prot
);

uintptr_t virtual_memory_map_mmio(uintptr_t mmio_paddr, uint64_t size);

void virtual_memory_flush();

void virtual_memory_flush_one(uint64_t vaddr);

void virtual_memory_traverse_leafs(
    uint64_t * table, uint32_t level,
    uint64_t table_base_vaddr,        // VA at table[0]
    uint64_t start, uint64_t end,
    void (*visit)(uint64_t * pte, uint64_t vaddr, void * ctx),
    void * ctx
);

void virtual_memory_free_tables(uint64_t * pgd, uint32_t start_index, uint32_t n);
