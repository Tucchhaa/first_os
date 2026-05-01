#pragma once

#include <stdint.h>

struct fdt_property {
    uint32_t len;
    uint32_t nameoff;
    uint8_t data;
};

const char * fdt_node_name(uintptr_t node_addr);

uintptr_t fdt_root_node();

uintptr_t fdt_sibling_node(uintptr_t node_addr);

uintptr_t fdt_child_node(uintptr_t node_addr);

uintptr_t fdt_parent_node(uintptr_t node_addr);

/// returns address of a node at the path
uintptr_t fdt_node_addr_by_path(const char * path);

uint8_t fdt_node_is_compatible(uintptr_t node_addr, const char * compatible);

uintptr_t fdt_node_addr_by_compatible(const char * compatible);

struct fdt_property * fdt_property_by_name(uintptr_t node_addr, const char * property_name);

/// returns a property with the property_name of the node
struct fdt_property * fdt_property_by_name(uintptr_t node_addr, const char * property_name);

void fdt_get_node_cells(uintptr_t node_addr, uint32_t * address_cells, uint32_t * size_cells);

void fdt_reg_property(uintptr_t node_addr, uint64_t * address, uint64_t * size);
