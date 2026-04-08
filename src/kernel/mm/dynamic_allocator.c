#include "dynamic_allocator.h"

#include "../ds/linked_list.h"
#include "page_allocator.h"

// for log only
#include "../../string.h"
#include "../../uart.h"

struct pool {
    uint32_t chunk_size;
    struct linked_list list;
};

# define POOLS_SIZE 8
uint32_t chunk_sizes[POOLS_SIZE] = { 16, 32, 48, 96, 128, 256, 512, 1024 };
struct pool pools[POOLS_SIZE];

void _log_allocate_chunk(uintptr_t addr, uint32_t chunk_size) {
    char b1[40], b2[40];
    i64tox(addr, b1);
    i32toa(chunk_size, b2);
    uart_puts_variadic("[Chunk] Allocate 0x", b1, " at chunk size ", b2, "\n", 0);
}

void _log_free_chunk(uintptr_t addr, uint32_t chunk_size) {
    char b1[40], b2[40];
    i64tox(addr, b1);
    i32toa(chunk_size, b2);
    uart_puts_variadic("[Chunk] Free 0x", b1, " at chunk size ", b2, "\n", 0);
}

void dynamic_allocator_init(void) {
    for (int i=0; i < POOLS_SIZE; i++) {
        pools[i].chunk_size = chunk_sizes[i];
        linked_list_init(&pools[i].list);
    }
}

uint8_t _get_pool_index(uint32_t size) {
    for (int i = 0; i < POOLS_SIZE; i++) {
        if (pools[i].chunk_size >= size) {
            return i;
        }
    }

    return 0xFF;
}

void * allocate(uint32_t size) {
    uint8_t pool_index = _get_pool_index(size);
    
    if (pool_index == 0xFF) {
        void * block_addr = memory_allocate_pages(size);

        if (block_addr == 0) {
            return 0;
        }

        struct page * page = memory_page_metadata((uintptr_t)block_addr);
        page->pool_index = 0xFF;

        _log_allocate_chunk((uintptr_t)block_addr, size);
        
        return block_addr;
    }

    struct pool * pool = &pools[pool_index];

    if (pool->list.head == 0) {
        void * block_addr = memory_allocate_pages(PAGE_SIZE);

        if (block_addr == 0) {
            return 0;
        }

        struct page * page = memory_page_metadata((uintptr_t)block_addr);
        page->pool_index = pool_index;

        for (uintptr_t addr = pool->chunk_size; addr < PAGE_SIZE; addr += pool->chunk_size) {
            uintptr_t chunk_addr = (uintptr_t)block_addr + addr;

            linked_list_insert(&pool->list, (struct linked_list_node *)chunk_addr); 
        }

        _log_allocate_chunk((uintptr_t)block_addr, pool->chunk_size);

        return block_addr;
    }

    struct linked_list_node * chunk = pool->list.head;

    linked_list_remove(&pool->list, chunk);

    _log_allocate_chunk((uintptr_t)chunk, pool->chunk_size);

    return (void *)chunk;
}

void free(void * block_addr) {
    struct page * page = memory_page_metadata((uintptr_t)block_addr);

    if (page == 0) {
        return;
    }

    if (page->pool_index == 0xFF) {
        _log_free_chunk((uintptr_t)block_addr, 1);
        memory_free_pages(block_addr);
        return;
    }

    struct pool * pool = &pools[page->pool_index];

    linked_list_insert(&pool->list, (struct linked_list_node *)block_addr);
    _log_free_chunk((uintptr_t)block_addr, pool->chunk_size);
}
