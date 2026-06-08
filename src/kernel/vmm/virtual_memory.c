#include "virtual_memory.h"

#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../../platform.h"
#include "../mm/dynamic_allocator.h"
#include "../mm/utils.h"
#include "definitions.h"

void set_virtual_kernel_offset(uint64_t value) {
    virtual_kernel_offset = value;
}

uint64_t get_satp_value(uint64_t pgd_paddr) {
    const uint64_t mode = (8UL << 60); // Sv39
    uint64_t value = (mode | (pgd_paddr >> 12));

    return value;
}

uint64_t va2pa(uint64_t virtual_addr) {
    return virtual_addr - virtual_kernel_offset;
}

uint64_t pa2va(uint64_t physical_addr) {
    return physical_addr + virtual_kernel_offset;
}

// leaf_level=0 - maps a 4KiB page
// leaf_level=1 - maps a 2MiB page
static void pagewalk(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr, 
    uint64_t prot,
    uint32_t leaf_level
) {
    uint64_t * current_table = pgd;

    for (uint32_t level = 2; level > leaf_level; level -= 1) {
        uint64_t index = (vaddr >> (12 + 9 * level)) & 0x1FF;
        uint64_t * entry = &current_table[index];

        if (*entry == 0 || (*entry & (PTE_READ | PTE_WRITE | PTE_EXECUTE))) {
            void * table = allocate(sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
            memzero(table, sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
            *entry = make_pte(va2pa((uint64_t)table), PTE_VALID);
        }

        current_table = (uint64_t *)pa2va((*entry >> 10) << 12);
    }

    uint64_t index = (vaddr >> (12 + 9 * leaf_level)) & 0x1FF;

    current_table[index] = make_pte(paddr, prot);
}

void virtual_memory_map(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr,
    uint64_t size, 
    uint64_t prot
) {
    for (uint64_t offset = 0; offset < size; offset += pte_mem_size) {
        pagewalk(pgd, vaddr + offset, paddr + offset, prot, 0);
    }

    // TOOD: why in virtual_memory_map_pmd it causes a stall?

}

void virtual_memory_map_pmd(
    uint64_t * pgd,
    uint64_t vaddr, 
    uint64_t paddr,
    uint64_t size, 
    uint64_t prot
) {
    for (uint64_t offset = 0; offset < size; offset += pmd_mem_size) {
        pagewalk(pgd, vaddr + offset, paddr + offset, prot, 1);
    }
}

uintptr_t virtual_memory_map_mmio(uintptr_t mmio_paddr, uint64_t size) {
    static uint64_t next_vaddr = 0;
    uintptr_t result_vaddr = virtual_mmio_offset + next_vaddr;

    for (uint64_t offset = 0; offset < size; offset += pte_mem_size) {
        pagewalk(
            kernel_pgd, 
            result_vaddr + offset, 
            mmio_paddr + offset, 
            PTE_MMIO_PROT, 
            0
        );

        next_vaddr += pte_mem_size;
    }

    virtual_memory_flush();

    return result_vaddr;
}

void virtual_memory_flush() {
    asm volatile ("sfence.vma zero, zero" ::: "memory");
}
