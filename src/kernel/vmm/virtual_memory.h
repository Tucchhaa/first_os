#pragma once

#include <stdint.h>

extern uint64_t virtual_kernel_offset;

uint64_t va2pa(uint64_t virtual_addr);

uint64_t pa2va(uint64_t physical_addr);

uint8_t virtual_memory_setup(uintptr_t fdt_addr);

void virtual_memory_drop_identity_mapping();

uintptr_t virtual_memory_map_mmio(uintptr_t mmio_paddr, uint64_t size);