#include "virtual_memory_setup.h"

#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../../platform.h"
#include "definitions.h"
#include "virtual_memory.h"

#include "../../uart/uart_sync.h"

extern char __kernel_start;
extern char __kernel_end;

static inline uint32_t get_pgd_index(uint64_t paddr) {
    return (paddr >> (12 + 18)) & 0x1FF;
}

static uint64_t memory_base;
static uint64_t memory_size;

uint8_t virtual_memory_setup(uintptr_t fdt_addr) {
    if (fdt_setup(fdt_addr)) {
        return 1;
    }

    uintptr_t memory_node_addr = fdt_node_addr_by_path("/memory");
    struct fdt_property * device_type_prop = fdt_property_by_name(memory_node_addr, "device_type");

    if (device_type_prop == 0 || streql("memory", &device_type_prop->data) == 0) {
        return 1;
    }

    uint64_t offset = 0;
    fdt_reg_property(memory_node_addr, &memory_base, &memory_size);

    for (; offset < memory_size; offset += pgd_mem_size) {
        uint64_t pgd_paddr = memory_base + offset;
        uint64_t pgd_vaddr = KERNEL_VIRTUAL_BASE + offset;

        kernel_pgd[get_pgd_index(pgd_paddr)] = make_pte(pgd_paddr, PTE_IDENTITY_FLAGS);
        kernel_pgd[get_pgd_index(pgd_vaddr)] = make_pte(pgd_paddr, PTE_KERNEL_FLAGS);
    }
    // TODO: needed for uart
    kernel_pgd[0] = make_pte(0, PTE_MMIO_FLAGS);

    virtual_mmio_offset = KERNEL_VIRTUAL_BASE + offset;

    uint64_t satp = get_satp_value(va2pa((uint64_t)kernel_pgd));
    asm volatile ("csrw satp, %0" :: "r"(satp) : "memory");
    asm volatile ("sfence.vma zero, zero");

    return 0;
}

void virtual_memory_drop_identity_mapping() {
    for (uint32_t i = 0; i < PAGE_TABLE_ENTRIES_NUM / 2; i++) {
        kernel_pgd[i] = 0;
    }

    asm volatile ("sfence.vma zero, zero" ::: "memory");
}

void virtual_memory_refine_mappings() {
    uint64_t kernel_vaddr_start = (uint64_t)&__kernel_start;
    uint64_t kernel_size = (uint64_t)&__kernel_end - kernel_vaddr_start;
    uint64_t kernel_paddr_start = KERNEL_PHYSICAL_ADDR;
    uint64_t kernel_paddr_end = kernel_paddr_start + kernel_size;

    virtual_memory_map_pmd(
        kernel_pgd, 
        kernel_vaddr_start, 
        kernel_paddr_start, 
        kernel_size, 
        PTE_KERNEL_FLAGS
    );

    if (kernel_paddr_start > memory_base) {
        virtual_memory_map_pmd(
            kernel_pgd,
            KERNEL_VIRTUAL_BASE,
            memory_base,
            kernel_paddr_start - memory_base,
            PTE_RAM_FLAGS
        );
    }

    uint64_t memory_paddr_end = memory_base + memory_size;

    if (kernel_paddr_end < memory_paddr_end) {
        uint64_t after_offset = kernel_paddr_end - memory_base;
        virtual_memory_map_pmd(
            kernel_pgd,
            KERNEL_VIRTUAL_BASE + after_offset,
            kernel_paddr_end,
            memory_paddr_end - kernel_paddr_end,
            PTE_RAM_FLAGS
        );
    }

    virtual_memory_ready = 1;

    asm volatile ("sfence.vma zero, zero" ::: "memory");
}
