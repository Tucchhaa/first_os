#include "page_allocator.h"
#include "../ds/linked_list.h"

#include "../../string.h"
#include "../../uart.h"

// 16GB
#define MAX_ORDERS 22

const uint8_t PAGE_FLAG_FREE = 1 << 0;
const uint8_t PAGE_FLAG_ALLOCATED = 1 << 1;
const uint8_t PAGE_FLAG_RESERVED = 1 << 2;

struct memory_region {
    uintptr_t addr;
    uint64_t size;
};

uint32_t available_memory_length = 0;
uint32_t available_memory_max_length = 10;
struct memory_region available_memory[10];

uint32_t reserved_memory_length = 0;
uint32_t reserved_memory_max_length = 10;
struct memory_region reserved_memory[100];

uint32_t frame_array_length = 0;
struct page * frame_array;

struct linked_list orders_lists[MAX_ORDERS];

uintptr_t memory_base_addr;

uint8_t need_logging = 0;

static inline uint32_t _get_page_index(uintptr_t page_addr) {
    // TODO: convert to bit operation
    return (page_addr - memory_base_addr) / PAGE_SIZE;
}

void _log_orders() {
    uint32_t counts[21];
    
    // Calculate counts for each order
    for (uint32_t order = 0; order <= 20; order++) {
        uint32_t count = 0;
        struct linked_list_node * current = orders_lists[order].head;
        
        while (current != 0) {
            count++;
            current = current->next;
        }
        counts[order] = count;
    }
    
    // Print orders line
    uart_puts_variadic("[Orders] orders ", 0);
    for (uint32_t order = 0; order <= 20; order++) {
        char order_buf[40];
        i32toa(order, order_buf);
        
        // Pad single digits with a leading space for alignment
        if (order < 10) {
            uart_puts_variadic(" ", order_buf, "  ", 0);
        } else {
            uart_puts_variadic(order_buf, "  ", 0);
        }
    }
    uart_puts_variadic("\n[Orders] values ", 0);
    
    // Print values line
    for (uint32_t order = 0; order <= 20; order++) {
        char count_buf[40];
        i32toa(counts[order], count_buf);
        
        // Get string length to calculate padding
        uint32_t len = kstrlen(count_buf);
        if (len == 1) {
            uart_puts_variadic(" ", count_buf, "  ", 0);
        } else {
            uart_puts_variadic(count_buf, "  ", 0);
        }
    }
    uart_puts_variadic("\n", 0);
}

void _log_page_add(uintptr_t page_addr, uint32_t order) {
    if (!need_logging) return;

    char b1[40], b2[40], b3[40];
    i32toa(_get_page_index(page_addr), b1);
    i64tox(page_addr, b2);
    i32toa(order, b3);
    uart_puts_variadic("[+] Add page ", b1, " at address 0x", b2, " to order ", b3, "\n", 0);
}

void _log_page_remove(uintptr_t page_addr, uint32_t order) {
    if (!need_logging) return;

    char b1[40], b2[40], b3[40];
    i32toa(_get_page_index(page_addr), b1);
    i64tox(page_addr, b2);
    i32toa(order, b3);
    uart_puts_variadic("[-] Remove page ", b1, " at address 0x", b2, " from order ", b3, "\n", 0);
}

void _log_buddy_found(uintptr_t page_addr, uintptr_t buddy_addr, uint32_t order) {
    if (!need_logging) return;

    char b1[40], b2[40], b3[40];
    i32toa(_get_page_index(buddy_addr), b1);
    i32toa(_get_page_index(page_addr), b2);
    i32toa(order, b3);
    uart_puts_variadic("[*] Buddy found! buddy idx: ", b1, " for page ", b2, " with order ", b3, "\n", 0);
}

void _log_page_allocate(uintptr_t page_addr, uint32_t order) {
    if (!need_logging) return;

    char b1[40], b2[40], b3[40];
    i64tox(page_addr, b1);
    i32toa(order, b2);
    i32toa(_get_page_index(page_addr), b3);
    uart_puts_variadic("[Page] Allocate 0x", b1, " at order ", b2, ", page ", b3, "\n", 0);

    _log_orders();
}

void _log_page_free(uintptr_t page_addr, uint32_t order) {
    if (!need_logging) return;

    char b1[40], b2[40], b3[40];
    i64tox(page_addr, b1);
    i32toa(order, b2);
    i32toa(_get_page_index(page_addr), b3);
    uart_puts_variadic("[Page] Free 0x", b1, " at order ", b2, ", page ", b3, "\n", 0);

    _log_orders();
}

void _log_reserve(uintptr_t addr, uint64_t size) {
    char b1[40], b2[40];
    i64tox(addr, b1);
    i64tox(addr + size, b2);
    uart_puts_variadic("[Reserve] Reserve address [0x", b1, ", 0x", b2, ")\n", 0);
}

void memory_add(uintptr_t addr, uint64_t size) {
    if (available_memory_length == available_memory_max_length) {
        return;
    }

    available_memory[available_memory_length].addr = addr;
    available_memory[available_memory_length].size = size;
    
    available_memory_length += 1;
}

void memory_reserve(uintptr_t addr, uint64_t size) {
    if (reserved_memory_length == reserved_memory_max_length) {
        return;
    }

    reserved_memory[reserved_memory_length].addr = addr;
    reserved_memory[reserved_memory_length].size = size;

    reserved_memory_length += 1;
}

struct page * memory_page_metadata(uintptr_t page_addr) {
    uint32_t page_index = _get_page_index(page_addr);

    if (page_index >= frame_array_length) {
        return (void *)0;
    }

    return &frame_array[page_index];
}

static void _memory_reserve_pages(uintptr_t addr, uint64_t size) {
    _log_reserve(addr, size);

    uint32_t reserved_page_num = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t index_offset = _get_page_index(addr);

    for (uint32_t i = 0; i < reserved_page_num; i++) {
        frame_array[index_offset + i].order = 0;
        frame_array[index_offset + i].flags = PAGE_FLAG_RESERVED;
    }
}

static uintptr_t _allocate_frame_array(uint32_t frame_array_size) {
    uintptr_t available_memory_end_addr = available_memory[0].addr + available_memory[0].size;
    uintptr_t frame_array_addr = available_memory[0].addr;

    while (1) {
        uint8_t is_overlapping = 0;
        uintptr_t frame_array_end_addr = frame_array_addr + frame_array_size;

        for (uint32_t i = 0; i < reserved_memory_length; i += 1) {
            uintptr_t reserved_memory_start_addr = reserved_memory[i].addr;
            uintptr_t reserved_memory_end_addr = reserved_memory[i].addr + reserved_memory[i].size;

            if (
                frame_array_addr < reserved_memory_end_addr
                && frame_array_end_addr > reserved_memory_start_addr
            ) {
                frame_array_addr = reserved_memory_end_addr;
                is_overlapping = 1;
                break;
            }
        }

        if (frame_array_end_addr > available_memory_end_addr) {
            return 0;
        }

        if (!is_overlapping) {
            return frame_array_addr;
        }
    }
}

static void _populate_orders_lists() {
    for (uint32_t i = 0; i < MAX_ORDERS; i++) {
        linked_list_init(&orders_lists[i]);
    }

    for (uint32_t i = 0; i < frame_array_length; i++) {
        struct page * page = &frame_array[i];
        
        if (page->flags & PAGE_FLAG_RESERVED) {
            continue;
        }

        if (page->flags & PAGE_FLAG_FREE) {
            continue; // already freed
        }

        uintptr_t page_addr = memory_base_addr + i * PAGE_SIZE;
        memory_free_pages((void *)page_addr);
    }
}

uint8_t memory_init() {
    // TODO: support multiple available memory regions
    uintptr_t available_memory_end_addr = available_memory[0].addr + available_memory[0].size;
    uint32_t page_count = available_memory->size / PAGE_SIZE;

    uint32_t frame_array_size = sizeof(struct page) * page_count;
    uintptr_t frame_array_addr = _allocate_frame_array(frame_array_size);

    if (frame_array_addr == 0) {
        return 1;
    }

    memory_base_addr = available_memory[0].addr;
    frame_array = (struct page *)frame_array_addr;
    frame_array_length = page_count;

    for (uint32_t i = 0; i < frame_array_length; i++) {
        frame_array[i].flags = 0;
        frame_array[i].order = 0;
        frame_array[i].pool_index = 0;
    }
    
    _memory_reserve_pages(frame_array_addr, frame_array_size);

    for (uint32_t i = 0; i < reserved_memory_length; i += 1) {
        if (reserved_memory[i].addr + reserved_memory[i].size > available_memory_end_addr) {
            continue;
        }

        if (reserved_memory[i].addr < memory_base_addr) {
            continue;
        }

        _memory_reserve_pages(reserved_memory[i].addr, reserved_memory[i].size);
    }

    _populate_orders_lists();

    need_logging = 1;

    return 0;
}

void * memory_allocate_pages(uint64_t size) {
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

    _log_page_allocate(block_addr, order);

    linked_list_remove(&orders_lists[order], orders_lists[order].head);
    _log_page_remove(block_addr, order);

    while (order > required_order) {
        order -= 1;
        uintptr_t buddy_addr = block_addr + (PAGE_SIZE << order);

        linked_list_insert(&orders_lists[order], (struct linked_list_node *)buddy_addr);
        _log_page_add(buddy_addr, order);

        struct page * buddy_page = memory_page_metadata(buddy_addr); 
        buddy_page->order = order;
        buddy_page->flags = PAGE_FLAG_FREE;
    }

    struct page * page = memory_page_metadata(block_addr);
    page->order = order;
    page->flags = PAGE_FLAG_ALLOCATED;

    return (void *)block_addr;
}

static struct linked_list_node * _get_buddy(uintptr_t block_addr, uint8_t order) {
    uintptr_t offset = block_addr - memory_base_addr;
    uintptr_t buddy_addr = (offset ^ (PAGE_SIZE << order)) + memory_base_addr;

    struct page * buddy_page = memory_page_metadata(buddy_addr);

    if (buddy_page == 0) {
        return 0;
    }

    if ((buddy_page->flags & PAGE_FLAG_FREE) && buddy_page->order == order) {
        _log_buddy_found(block_addr, buddy_addr, order);

        return (struct linked_list_node *)buddy_addr;
    }

    return 0;
}

void memory_free_pages(void * block_addr) {
    struct page * page = memory_page_metadata((uintptr_t)block_addr);

    if (page == 0) {
        return;
    }

    uint8_t order = page->order;
    uintptr_t new_block_addr = (uintptr_t)block_addr;

    _log_page_free(new_block_addr, order);

    while (order < MAX_ORDERS - 1) {
        struct linked_list_node * buddy = _get_buddy(new_block_addr, order);

        if (buddy == 0) {
            break;
        }

        linked_list_remove(&orders_lists[order], buddy);
        _log_page_remove((uintptr_t)buddy, order);

        new_block_addr = (uintptr_t)buddy < new_block_addr ? (uintptr_t)buddy : new_block_addr;
        order += 1;
    }

    linked_list_insert(&orders_lists[order], (struct linked_list_node *)new_block_addr);
    _log_page_add(new_block_addr, order);

    struct page * new_page = memory_page_metadata(new_block_addr);
    new_page->order = order;
    new_page->flags = PAGE_FLAG_FREE;
}
