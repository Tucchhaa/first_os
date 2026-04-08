#pragma once

#include <stdint.h>

#define PAGE_SIZE 4096 // 4kb

struct page {
    uint8_t order;
    uint8_t flags;
    uint8_t pool_index;
};

struct page * memory_page_metadata(uintptr_t page_addr);

void memory_init();

void memory_insert(uintptr_t addr, uint64_t size);

void memory_reserve(uintptr_t addr, uint64_t size);

void * memory_allocate_pages(uint64_t size);

void memory_free_pages(void * block_addr);
