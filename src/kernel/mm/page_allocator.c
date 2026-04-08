#include "page_allocator.h"
#include "../ds/linked_list.h"

#include "../../string.h"
#include "../../uart.h"

// 16GB
#define MAX_ORDERS 22

const uint32_t PAGE_SIZE = 4096; // 4kb

struct page {
    uint8_t order;
    uint8_t flags;
};

const uint8_t PAGE_FREE = 1 << 0;
const uint8_t PAGE_ALLOCATED = 1 << 1;
const uint8_t PAGE_RESERVED = 1 << 2;

uint32_t frame_array_size = 0;
struct page frame_array[1500000];

struct linked_list orders_lists[MAX_ORDERS];

uintptr_t memory_base_addr;

uint8_t need_log_status = 0;
void _log_status() {
    if (need_log_status == 0) {
        return;
    }

    for (int i=0; i < MAX_ORDERS; i++) {
        uint32_t count = 0;
        struct linked_list_node * current_node = orders_lists[i].head;

        while (current_node != 0) {
            count += 1;
            current_node = current_node->next;
        }

        char buf1[32], buf2[32];
        i32toa(i, buf1);
        i32toa(count, buf2);

        uart_puts_variadic("order ", buf1, ": ", buf2, " nodes\n", 0);
    }
}

static inline uint32_t _get_page_index(uintptr_t page_addr) {
    // TODO: convert to bit operation
    return (page_addr - memory_base_addr) / PAGE_SIZE;
}

static struct page * _get_page(uintptr_t page_addr) {
    uint32_t page_index = _get_page_index(page_addr);

    if (page_index >= frame_array_size) {
        return (void *)0;
    }

    return &frame_array[page_index];
}

void memory_init() {
    for (uint32_t i = 0; i < MAX_ORDERS; i++) {
        linked_list_init(&orders_lists[i]);
    }

    char b[32];
    i32toa(frame_array_size, b);
    uart_puts_variadic("frame array size: ", b, "\n", 0);

    for (uint32_t i = 0; i < frame_array_size; i++) {
        struct page * page = &frame_array[i];
        
        if (page->flags & PAGE_RESERVED) {
            continue;
        }

        if (page->flags & PAGE_FREE) {
            continue; // already freed
        }

        uintptr_t page_addr = memory_base_addr + i * PAGE_SIZE;
        memory_free_pages(page_addr);
    }

    need_log_status = 1;
    _log_status();
}

void memory_insert(uintptr_t addr, uint64_t size) {
    memory_base_addr = addr;
    frame_array_size = size / PAGE_SIZE;
}

void memory_reserve(uintptr_t addr, uint64_t size) {
    uint32_t index_offset = _get_page_index(addr);

    for (uint32_t i = 0; i < size / PAGE_SIZE; i++) {
        frame_array[index_offset + i].order = 0;
        frame_array[index_offset + i].flags = PAGE_RESERVED;
    }
}

uintptr_t memory_allocate_pages(uint64_t size) {
    uint32_t required_order = 0;
    uint64_t current_size = PAGE_SIZE;

    while (current_size < size) {
        required_order += 1;
        current_size <<= 1;
    }

    uint32_t order = required_order;

    while (order < MAX_ORDERS && orders_lists[order].head == 0) {
        order += 1;
    }

    if (order >= MAX_ORDERS) {
        return 0;
    }

    uintptr_t block_addr = (uintptr_t)orders_lists[order].head;

    linked_list_remove(&orders_lists[order], orders_lists[order].head);

    while (order > required_order) {
        order -= 1;
        uintptr_t buddy_addr = block_addr + (PAGE_SIZE << order);

        linked_list_insert(&orders_lists[order], (struct linked_list_node *)buddy_addr);

        struct page * buddy_page = _get_page(buddy_addr); 
        buddy_page->order = order;
        buddy_page->flags = PAGE_FREE;
    }

    struct page * page = _get_page(block_addr);
    page->order = order;
    page->flags = PAGE_ALLOCATED;

    _log_status();

    return block_addr;
}

static struct linked_list_node * _get_buddy(uintptr_t block_addr, uint8_t order) {
    uintptr_t offset = block_addr - memory_base_addr;
    uintptr_t buddy_addr = (offset ^ (PAGE_SIZE << order)) + memory_base_addr;

    struct page * buddy_page = _get_page(buddy_addr);

    if (buddy_page == 0) {
        return 0;
    }

    if ((buddy_page->flags & PAGE_FREE) && buddy_page->order == order) {
        return (struct linked_list_node *)buddy_addr;
    }

    return 0;
}

void memory_free_pages(uintptr_t block_addr) {
    struct page * page = _get_page(block_addr);

    if (page == 0) {
        return;
    }

    uint8_t order = page->order;
    uintptr_t new_block_addr = block_addr;

    while (order < MAX_ORDERS - 1) {
        struct linked_list_node * buddy = _get_buddy(new_block_addr, order);

        if (buddy == 0) {
            break;
        }

        linked_list_remove(&orders_lists[order], buddy);

        new_block_addr = (uintptr_t)buddy < new_block_addr ? (uintptr_t)buddy : new_block_addr;
        order += 1;
    }

    linked_list_insert(&orders_lists[order], (struct linked_list_node *)new_block_addr);

    struct page * new_page = _get_page(new_block_addr);
    new_page->order = order;
    new_page->flags = PAGE_FREE;

    _log_status();
}
