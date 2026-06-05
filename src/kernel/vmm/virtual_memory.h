#pragma once

#include <stdint.h>

extern uint64_t kernel_pgd[512];
extern uint8_t virtual_memory_ready;

void set_virtual_kernel_offset(uint64_t value);

uint64_t get_satp_value(uint64_t pgd_paddr);

uint64_t va2pa(uint64_t virtual_addr);
uint64_t pa2va(uint64_t physical_addr);

void virtual_memory_map(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr,
    uint64_t size, 
    uint64_t flags
);

void virtual_memory_map_pmd(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr,
    uint64_t size, 
    uint64_t flags
);

uintptr_t virtual_memory_map_mmio(uintptr_t mmio_paddr, uint64_t size);
