#pragma once

#include <stdint.h>

uint8_t virtual_memory_setup(uintptr_t fdt_addr);

void virtual_memory_drop_identity_mapping();

void virtual_memory_refine_mappings();
