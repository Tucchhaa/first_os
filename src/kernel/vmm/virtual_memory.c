#include "virtual_memory.h"

#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../../platform.h"
#include "../mm/page_allocator.h"
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

uint64_t * vaddr_from_pte(uint64_t pte) {
    // bits 10..53
    const uint64_t PTE_PPN_MASK = ((1ULL << 44) - 1) << 10; 
    uint64_t paddr = ((pte & PTE_PPN_MASK) >> 10) << 12;
    return (uint64_t *)pa2va(paddr);
}

static inline uint8_t is_leaf_pte(uint64_t pte) {
    return (pte & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) != 0;
}

static inline uint64_t level_span(uint32_t level) {
    return 1ULL << (12 + 9 * level);
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

        // TODO: entry is leaf only when refining kernel memory
        if (*entry == 0 || (*entry & (PTE_READ | PTE_WRITE | PTE_EXECUTE))) {
            void * table = memory_allocate_pages(sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
            memzero(table, sizeof(uint64_t) * PAGE_TABLE_ENTRIES_NUM);
            *entry = make_pte(va2pa((uint64_t)table), PTE_VALID);
        }

        current_table = vaddr_from_pte(*entry);
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

void virtual_memory_flush_one(uint64_t vaddr) {
    asm volatile ("sfence.vma %0, zero" :: "r"(vaddr) : "memory");
}

void virtual_memory_traverse_leafs(
    uint64_t * table, uint32_t level,
    uint64_t table_base_vaddr,        // VA at table[0]
    uint64_t start, uint64_t end,
    void (*visit)(uint64_t * pte, uint64_t vaddr, void * ctx),
    void * ctx
) {
    uint64_t span = level_span(level);

    for (uint32_t i = 0; i < PAGE_TABLE_ENTRIES_NUM; i++) {
        uint64_t entry_vaddr = table_base_vaddr + (uint64_t)i * span;
        uint64_t entry_end   = entry_vaddr + span;

        if (entry_end <= start) continue;
        if (entry_vaddr >= end) break;

        uint64_t pte = table[i];

        if (pte == 0 || !(pte & PTE_VALID)) continue;

        if (is_leaf_pte(pte)) {
            visit(&table[i], entry_vaddr, ctx);
            continue;
        }

        uint64_t * sub = vaddr_from_pte(pte);
        virtual_memory_traverse_leafs(sub, level - 1, entry_vaddr, start, end, visit, ctx);
    }
}

void virtual_memory_free_tables(uint64_t * table, uint32_t start_index, uint32_t n) {
    for (uint32_t i = start_index; i < start_index + n; i += 1) {
        uint64_t pte = table[i];

        if (pte == 0 || is_leaf_pte(pte)) {
            table[i] = 0;
            continue;
        }

        uint64_t * sub_table = vaddr_from_pte(pte);

        virtual_memory_free_tables(sub_table, 0, PAGE_TABLE_ENTRIES_NUM);
        memory_free_pages((void *)sub_table);

        table[i] = 0;
    }
}
