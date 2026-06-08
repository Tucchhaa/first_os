#pragma once

#include <stdint.h>

#define PAGE_TABLE_ENTRIES_NUM 512

static const uint64_t KERNEL_VIRTUAL_BASE =  0xFFFFFFC000000000ULL;

extern uint64_t kernel_pgd[PAGE_TABLE_ENTRIES_NUM];

extern uint64_t virtual_mmio_offset;
extern uint64_t virtual_kernel_offset;
extern uint8_t virtual_memory_ready;

static const uint64_t pgd_mem_size = 1ULL << 30; // 1 GiB
static const uint64_t pmd_mem_size = 1ULL << 21; // 2 MiB
static const uint64_t pte_mem_size = 1ULL << 12; // 4 KiB

static const uint8_t PTE_VALID = (1 << 0);
static const uint8_t PTE_READ = (1 << 1);
static const uint8_t PTE_WRITE = (1 << 2);
static const uint8_t PTE_EXECUTE = (1 << 3);
static const uint8_t PTE_USER = (1 << 4);
static const uint8_t PTE_GLOBAL = (1 << 5);
static const uint8_t PTE_ACCESSED = (1 << 6);
static const uint8_t PTE_DIRTY = (1 << 7);

static const uint8_t PTE_IDENTITY_PROT = 
    PTE_VALID | PTE_READ | PTE_WRITE | PTE_EXECUTE | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_RAM_PROT = 
    PTE_VALID | PTE_READ | PTE_WRITE | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_MMIO_PROT = 
    PTE_VALID | PTE_READ | PTE_WRITE | PTE_GLOBAL | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_KERNEL_PROT = 
    PTE_VALID | PTE_READ | PTE_WRITE | PTE_GLOBAL | PTE_EXECUTE | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_USER_CODE_PROT =
    PTE_VALID | PTE_READ | PTE_EXECUTE | PTE_USER | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_USER_STACK_PROT =
    PTE_VALID | PTE_READ | PTE_WRITE | PTE_USER | PTE_ACCESSED | PTE_DIRTY;

static inline uint64_t make_pte(uint64_t paddr, uint8_t prot) {
    return ((paddr >> 12) << 10) | prot;
}

static inline uint64_t align_to_pte(uint64_t addr) {
    return (addr + pte_mem_size - 1) & ~(pte_mem_size - 1);
}
