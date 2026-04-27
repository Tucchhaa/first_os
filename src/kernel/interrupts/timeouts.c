#include "timeouts.h"

#include "interrupts.h"
#include "../ds/linked_list.h"
#include "../mm/dynamic_allocator.h"
#include "../../fdt/fdt.h"
#include "../../uart_sync.h"
#include "../../utils.h"
#include "../sbi.h"

static uint32_t cpu_frequency = 0;

static struct linked_list timeout_queue;

struct timeout_entry {
    struct linked_list_node node;
    void (*callback)(void *);
    void * arg;
    uint64_t target_time;
};

static void _insert_to_timeouts_queue(struct timeout_entry *entry);

void timeouts_setup() {
    linked_list_init(&timeout_queue);

    uintptr_t cpus_node_addr = fdt_node_addr_by_path("/cpus");
    struct fdt_property * cpu_frequency_prop = fdt_property_by_name(cpus_node_addr, "timebase-frequency");

    if (cpu_frequency_prop == (void *)0) {
        uart_sync_puts("[KERNEL:TIMEOUTS] /cpus[timebase-frequency] not found\n");
        return;
    }

    cpu_frequency = be32_to_cpu(*(uint32_t *)(&cpu_frequency_prop->data));
}

void set_timeout(void (*callback)(void *), void * arg, uint64_t timeout_ms) {
    struct timeout_entry * entry = allocate(sizeof(struct timeout_entry));
    entry->callback = callback;
    entry->arg = arg;
    entry->target_time = sbi_read_time() + (cpu_frequency / 1000) * timeout_ms;

    uint8_t prev = interrupts_disable();

    _insert_to_timeouts_queue(entry);   

    if (timeout_queue.head == &entry->node) {
        sbi_set_timer(entry->target_time);
    }

    if (prev) {
        interrupts_enable();
    }
}

void timeouts_interrupt_handler() {
    uint64_t now = sbi_read_time();

    while (timeout_queue.head != 0) {
        struct timeout_entry * timeout_entry = (struct timeout_entry *)timeout_queue.head;

        if (timeout_entry->target_time > now) break;

        linked_list_remove(&timeout_queue, &timeout_entry->node);

        timeout_entry->callback(timeout_entry->arg);

        free(timeout_entry);
    }

    if (timeout_queue.head != 0) {
        struct timeout_entry * next = (struct timeout_entry *)timeout_queue.head;
        sbi_set_timer(next->target_time);
    } else {
        sbi_set_timer(sbi_read_time() + cpu_frequency * 10);
    }
}

static void _insert_to_timeouts_queue(struct timeout_entry *entry) {
    struct linked_list_node *current = timeout_queue.head;

    while (current != 0) {
        struct timeout_entry *current_entry = (struct timeout_entry *)current;

        if (entry->target_time < current_entry->target_time) {
            entry->node.next = current;
            entry->node.prev = current->prev;

            if (current->prev != 0) {
                current->prev->next = &entry->node;
            } else {
                timeout_queue.head = &entry->node;
            }
            current->prev = &entry->node;
            return;
        }

        current = current->next;
    }

    linked_list_insert(&timeout_queue, &entry->node);
}
