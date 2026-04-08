#pragma once

#include <stdint.h>

struct fdt_header {
    // big-endian 0xd00dfeed
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_property {
    uint32_t len;
    uint32_t nameoff;
    uint8_t data;
};

struct fdt_node_cells {
    uint8_t error;
    uint32_t address;
    uint32_t size;
};

uint32_t fdt_check_magic(uintptr_t fdt);

uint64_t fdt_total_size(uintptr_t fdt);

const char * fdt_node_name(uintptr_t node_addr);

/// returns address of a node at the path
uintptr_t fdt_node_addr_by_path(uintptr_t fdt, const char * path);

uintptr_t fdt_sibling_node(uintptr_t node_addr);

uintptr_t fdt_child_node(uintptr_t node_addr);

struct fdt_property * fdt_property_at_addr(uintptr_t property_addr);

/// returns a property with the property_name of the node
uintptr_t fdt_property_addr_by_name(
    uintptr_t fdt, 
    uintptr_t node_addr,
    const char * property_name
);

struct fdt_node_cells fdt_get_node_cells(uintptr_t fdt, uintptr_t node_addr);

void fdt_read_reg_property(
    uintptr_t fdt, uintptr_t node_addr, 
    uint32_t address_cells, uint32_t size_cells,
    uint64_t * address, uint64_t * size
);
