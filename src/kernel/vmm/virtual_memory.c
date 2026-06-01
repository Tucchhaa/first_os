#include "virtual_memory.h"

#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../../platform.h"

#define ENTRIES_PER_TABLE 512

static uint64_t pgd[ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pmd[32][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pte[64][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));

static uint8_t pmd_pool_size = 0;
static uint8_t pte_pool_size = 0;
static uint8_t mmio_pmd_pool_idx = 0;
static uint16_t pmd_pte_map[ENTRIES_PER_TABLE];

static const uint64_t pgd_mem_size = 1ULL << 30; // 1 GiB
static const uint64_t pmd_mem_size = 1ULL << 21; // 2 MiB
static const uint64_t pte_mem_size = 1ULL << 12; // 4 KiB

static inline uint64_t get_pgd_index(uint64_t paddr) {
    return (paddr >> (12 + 18)) & 0x1FF;
}

static inline uint64_t get_pmd_index(uint64_t paddr) {
    return (paddr >> (12 + 9)) & 0x1FF;
}

static inline uint64_t get_pte_index(uint64_t paddr) {
    return (paddr >> 12) & 0x1FF;
}

static inline uint64_t make_pte(uint64_t paddr, uint8_t flags) {
    return ((paddr >> 12) << 10) | flags;
}

static const uint8_t PTE_VALID = (1 << 0);
static const uint8_t PTE_READ = (1 << 1);
static const uint8_t PTE_WRITE = (1 << 2);
static const uint8_t PTE_EXECUTE = (1 << 3);
static const uint8_t PTE_USER = (1 << 4);
static const uint8_t PTE_GLOBAL = (1 << 5);
static const uint8_t PTE_ACCESSED = (1 << 6);
static const uint8_t PTE_DIRTY = (1 << 7);

static const uint64_t KERNEL_VIRTUAL_BASE =  0xFFFFFFC000000000ULL;

static inline void set_page_table(uint64_t table_paddr) {
    const uint64_t mode = (8UL << 60); // Sv39
    uint64_t val = (mode | (table_paddr >> 12));

    asm volatile ("csrw satp, %0" :: "r"(val) : "memory");
    asm volatile ("sfence.vma zero, zero");
}

static const uint8_t PTE_IDENTITY_FLAGS = PTE_VALID | PTE_READ | PTE_WRITE | PTE_EXECUTE | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_RAM_FLAGS = PTE_VALID | PTE_READ | PTE_WRITE | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_MMIO_FLAGS = PTE_VALID | PTE_READ | PTE_WRITE | PTE_GLOBAL | PTE_ACCESSED | PTE_DIRTY;
static const uint8_t PTE_KERNEL_FLAGS = PTE_VALID | PTE_READ | PTE_WRITE | PTE_GLOBAL | PTE_EXECUTE | PTE_ACCESSED | PTE_DIRTY;

extern char __kernel_start;
extern char __kernel_end;

static uint64_t virtual_mmio_offset = 0;
uint64_t virtual_kernel_offset = 0;

uint64_t va2pa(uint64_t virtual_addr) {
    return virtual_addr - virtual_kernel_offset;
}

uint64_t pa2va(uint64_t physical_addr) {
    return physical_addr + virtual_kernel_offset;
}

uint8_t virtual_memory_setup(uintptr_t fdt_addr) {
    if (fdt_setup(fdt_addr)) {
        return 1;
    }

    uintptr_t memory_node_addr = fdt_node_addr_by_path("/memory");
    struct fdt_property * device_type_prop = fdt_property_by_name(memory_node_addr, "device_type");

    if (device_type_prop == 0 || streql("memory", &device_type_prop->data) == 0) {
        return 1;
    }

    uint64_t memory_base = 0, memory_size = 0;
    fdt_reg_property(memory_node_addr, &memory_base, &memory_size);

    // Map RAM to identity and to kernel space
    for (
        uint64_t pgd_paddr = memory_base;
        pgd_paddr < memory_base + memory_size;
        pgd_paddr += pgd_mem_size
    ) {
        uint64_t identity_pgd_index = get_pgd_index(pgd_paddr);
        pgd[identity_pgd_index] = make_pte(pgd_paddr, PTE_IDENTITY_FLAGS);
        
        uint64_t pgd_index = get_pgd_index(KERNEL_VIRTUAL_BASE + (pgd_paddr - memory_base));
        pgd[pgd_index] = make_pte((uint64_t)&pmd[pmd_pool_size], PTE_VALID);

        for (
            uint64_t pmd_paddr = pgd_paddr;
            pmd_paddr < pgd_paddr + pgd_mem_size;
            pmd_paddr += pmd_mem_size
        ) {
            uint64_t pmd_index = get_pmd_index(pmd_paddr);

            pmd[pmd_pool_size][pmd_index] = make_pte(pmd_paddr, PTE_RAM_FLAGS);
        }

        pmd_pool_size += 1;
    }

    // Map MMIO
    {
        virtual_mmio_offset = KERNEL_VIRTUAL_BASE + pmd_pool_size * pgd_mem_size;
        mmio_pmd_pool_idx = pmd_pool_size++;

        uint16_t mmio_pgd_index = get_pgd_index(virtual_mmio_offset);
        pgd[mmio_pgd_index] = make_pte((uint64_t)&pmd[mmio_pmd_pool_idx], PTE_VALID);
    }

    // Map kernel
    uint64_t kernel_size = (uint64_t)&__kernel_end - (uint64_t)&__kernel_start;

    for (
        uint64_t pmd_paddr = KERNEL_PHYSICAL_ADDR;
        pmd_paddr < KERNEL_PHYSICAL_ADDR + kernel_size;
        pmd_paddr += pmd_mem_size
    ) {
        uint64_t pgd_i = get_pgd_index(pmd_paddr) - get_pgd_index(memory_base);
        uint64_t pmd_index = get_pmd_index(pmd_paddr);

        pmd[pgd_i][pmd_index] = make_pte(pmd_paddr, PTE_KERNEL_FLAGS);
    }

    set_page_table((uint64_t)pgd);

    return 0;
}

void virtual_memory_drop_identity_mapping() {
    for (uint32_t i = 0; i < ENTRIES_PER_TABLE / 2; i++) {
        pgd[i] = 0;
    }

    asm volatile ("sfence.vma zero, zero" ::: "memory");
}

uintptr_t virtual_memory_map_mmio(uintptr_t mmio_paddr, uint64_t size) {
    static uint64_t next_vaddr = 0;
    
    uintptr_t result_vaddr = virtual_mmio_offset + next_vaddr;
    uint64_t offset = 0;
    
    for (; offset < size; offset += pte_mem_size) {
        uint64_t vaddr = virtual_mmio_offset + next_vaddr;
        uint64_t paddr = mmio_paddr + offset;
        
        uint64_t pmd_idx = get_pmd_index(vaddr);
        uint64_t pte_idx = get_pte_index(vaddr);
        
        if (pmd[mmio_pmd_pool_idx][pmd_idx] == 0) {
            uint64_t pte_paddr = va2pa((uint64_t)&pte[pte_pool_size]);
            pmd_pte_map[pmd_idx] = pte_pool_size;
            pmd[mmio_pmd_pool_idx][pmd_idx] = make_pte(pte_paddr, PTE_VALID);
            pte_pool_size += 1;
        }

        uint16_t pte_pool_idx = pmd_pte_map[pmd_idx];
        pte[pte_pool_idx][pte_idx] = make_pte(paddr, PTE_MMIO_FLAGS);
        
        next_vaddr += pte_mem_size;
    }
    
    asm volatile ("sfence.vma zero, zero" ::: "memory");
    
    return result_vaddr;
}
