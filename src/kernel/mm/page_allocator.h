#include <stdint.h>

void memory_init();

void memory_insert(uintptr_t addr, uint64_t size);

void memory_reserve(uintptr_t addr, uint64_t size);

uintptr_t memory_allocate_pages(uint64_t size);

void memory_free_pages(uintptr_t block_addr);
