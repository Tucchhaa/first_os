#include "definitions.h"

uint64_t kernel_pgd[512] __attribute__((aligned(4096)));

uint64_t virtual_mmio_offset = 0;
uint64_t virtual_kernel_offset = 0;
uint8_t virtual_memory_ready = 0;
