#include "fdt_parser.h"
#include "fdt.h"
#include "../string.h"
#include "../converters.h"

/*
Token values are already converted from big-endian to little-endian
*/
static const uint32_t FDT_TOKEN_BEGIN_NODE = 0x01000000;
static const uint32_t FDT_TOKEN_END_NODE = 0x02000000;
static const uint32_t FDT_TOKEN_PROP = 0x03000000;
static const uint32_t FDT_TOKEN_NOP = 0x04000000;
static const uint32_t FDT_TOKEN_END = 0x09000000;

static const uint32_t TOKEN_SIZE = sizeof(uint32_t);

/// returns addr of the next token
static uintptr_t _fdt_next_token_addr(uintptr_t token_addr) {
    uintptr_t next_token_addr = token_addr;

    if (*(uint32_t *)token_addr == FDT_TOKEN_PROP) {
        struct fdt_property * property = (struct fdt_property *)(token_addr + TOKEN_SIZE);
        next_token_addr = token_addr
            + TOKEN_SIZE
            + sizeof(property->len) 
            + sizeof(property->nameoff) 
            + be32_to_cpu(property->len);

        next_token_addr = (next_token_addr + 3) & ~3;
    }
    else if(*(uint32_t *)next_token_addr == FDT_TOKEN_BEGIN_NODE) {
        next_token_addr += TOKEN_SIZE;

        // skip node name
        while (*(char *)next_token_addr != '\0') {
            next_token_addr += sizeof(char);
        }
        next_token_addr += sizeof(char); // skip \0 byte

        next_token_addr = (next_token_addr + 3) & ~3;
    }
    else if (*(uint32_t *)next_token_addr == FDT_TOKEN_END_NODE) { 
        next_token_addr += TOKEN_SIZE; 
    }

    while (*(uint32_t *)next_token_addr == FDT_TOKEN_NOP) { 
        next_token_addr += TOKEN_SIZE; 
    }

    return next_token_addr;
}

const char * fdt_node_name(uintptr_t node_addr) {
    return (const char *)(node_addr + TOKEN_SIZE);
}

const char * fdt_property_name(struct fdt_property * property) {
    uint32_t fdt_strings_offset = be32_to_cpu(fdt_header()->off_dt_strings);
    uint32_t nameoff = be32_to_cpu(property->nameoff);

    return (char *)(fdt_addr + fdt_strings_offset + nameoff);
}

uintptr_t fdt_root_node() {
    struct fdt_header * header = fdt_header();

    if (header == (void *)0) {
        return 0;
    }

    uint32_t fdt_data_offset = be32_to_cpu(header->off_dt_struct);
    uintptr_t start_addr = fdt_addr + fdt_data_offset;

    if (*(uint32_t *)start_addr == FDT_TOKEN_BEGIN_NODE) {
        return start_addr;
    }

    uintptr_t token_addr = _fdt_next_token_addr(start_addr);

    if (*(uint32_t *)token_addr == FDT_TOKEN_BEGIN_NODE) {
        return token_addr;
    }

    return 0;
}

uintptr_t fdt_sibling_node(uintptr_t node_addr) {
    if (node_addr == 0) {
        return 0;
    }

    uintptr_t current_addr = node_addr;
    int32_t depth = 1;

    while (1) {
        current_addr = _fdt_next_token_addr(current_addr);

        if (*(uint32_t *)current_addr == FDT_TOKEN_BEGIN_NODE) {
            depth += 1;

            if (depth == 1) {
                return current_addr;
            }
        } 
        else if (*(uint32_t *)current_addr == FDT_TOKEN_END_NODE) {
            depth -= 1;

            if (depth == -1) {
                return 0;
            }
        } 
        else if (*(uint32_t *)current_addr == FDT_TOKEN_END) {
            return 0;
        }
    }
}

uintptr_t fdt_child_node(uintptr_t node_addr) {
    if (node_addr == 0) {
        return 0;
    }

    uintptr_t current_addr = node_addr;

    while (1) {
        current_addr = _fdt_next_token_addr(current_addr);

        if (*(uint32_t *)current_addr == FDT_TOKEN_BEGIN_NODE) {
            return current_addr;
        }
        else if (
            *(uint32_t *)current_addr == FDT_TOKEN_END_NODE ||
            *(uint32_t *)current_addr == FDT_TOKEN_END
        ) {
            return 0;
        } 
    }
}

uintptr_t fdt_parent_node(uintptr_t node_addr) {
    uintptr_t current_addr = fdt_root_node();

    if (node_addr == 0 || node_addr == current_addr) {
        return 0;
    }

    uintptr_t depth = 1;
    uintptr_t parent_stack[32];
    parent_stack[0] = 0;
    parent_stack[1] = current_addr;

    while (1) {
        current_addr = _fdt_next_token_addr(current_addr);

        if (*(uint32_t *)current_addr == FDT_TOKEN_BEGIN_NODE) {
            parent_stack[depth + 1] = current_addr;
            depth += 1;

            if (current_addr == node_addr) {
                return parent_stack[depth - 1];
            }
        }
        else if (*(uint32_t *)current_addr == FDT_TOKEN_END_NODE) {
            depth -= 1;
        }
        else if (*(uint32_t *)current_addr == FDT_TOKEN_END) {
            return 0;
        }
    }
}

/// if addr is node address, then returns first property
/// if addr is property address, then returns next property
/// if there is no property found, returns 0
static uintptr_t _fdt_next_property_addr(uintptr_t addr) {
    uintptr_t property_addr = _fdt_next_token_addr(addr);

    if (*(uint32_t *)property_addr == FDT_TOKEN_PROP) {
        return property_addr;
    }

    return 0;
}

uintptr_t fdt_node_addr_by_path(const char * path) {
    uintptr_t current_addr = fdt_root_node();

    if (current_addr == 0 || path[0] != '/') {
        return 0;
    }

    const char * p = path;
    uint32_t q = 0;

    while (1) {
        const char * current_node_name = fdt_node_name(current_addr);

        if (
            streqln(current_node_name, p, q) &&
            (
                current_node_name[q] == '\0' ||
                current_node_name[q] == '@'
            )
        ) {
            p += q;
            p += (*p == '/' ? 1 : 0);

            q = strtoken(p, "/", 0);

            if (*p == '\0') {
                return current_addr;
            }

            current_addr = fdt_child_node(current_addr);
        } 
        else {
            current_addr = fdt_sibling_node(current_addr);
        }

        if (current_addr == 0) {
            return 0;
        }
    }
}

uintptr_t fdt_node_addr_by_compatible(const char * compatible) {
    uintptr_t current_addr = fdt_root_node();

    while (1) {
        current_addr = _fdt_next_token_addr(current_addr);

        if (*(uint32_t *)current_addr == FDT_TOKEN_BEGIN_NODE) {
            struct fdt_property * compatible_prop = fdt_property_by_name(current_addr, "compatible");

            if (compatible_prop == (void *)0) {
                continue;
            }

            uint32_t len = be32_to_cpu(compatible_prop->len);
            const char * s = (const char *)&compatible_prop->data;

            uint32_t i = 0;

            while (i < len) {
                if (streql(&s[i], compatible)) {
                    return current_addr;
                }

                while (i < len && s[i] != '\0') {
                    i += 1;
                }
                i += 1;
            }
        }
        else if (*(uint32_t *)current_addr == FDT_TOKEN_END) {
            return 0;
        }
    }
}

struct fdt_property * fdt_property_by_name(uintptr_t node_addr, const char * property_name) {
    if (node_addr == 0) {
        return 0;
    }

    uintptr_t current_addr = node_addr;

    while (1) {
        current_addr = _fdt_next_property_addr(current_addr);

        if (current_addr == 0) {
            return 0;
        }

        struct fdt_property * current_property = (struct fdt_property *)(current_addr + TOKEN_SIZE);
        const char * current_property_name = fdt_property_name(current_property);

        if (streql(current_property_name, property_name)) {
            return current_property;
        }
    }
}

void fdt_get_node_cells(uintptr_t node_addr, uint32_t * address_cells, uint32_t * size_cells) {
    uintptr_t current_addr = node_addr;

    while (1) {
        uintptr_t parent_node_addr = fdt_parent_node(current_addr);

        if (node_addr == 0) {
            *address_cells = 0;
            *size_cells = 0;
            return;
        }

        struct fdt_property * address_cells_prop = fdt_property_by_name(parent_node_addr, "#address-cells");
        struct fdt_property * size_cells_prop = fdt_property_by_name(parent_node_addr, "#size-cells");

        if (address_cells_prop != 0 && size_cells_prop != 0) {
            *address_cells = be32_to_cpu(*(uint32_t *)(&address_cells_prop->data));
            *size_cells = be32_to_cpu(*(uint32_t *)(&size_cells_prop->data));
            return;
        }
    }
} 

void fdt_reg_property(uintptr_t node_addr, uint64_t * address, uint64_t * size) {
    struct fdt_property * reg_prop = fdt_property_by_name(node_addr, "reg");

    uint32_t address_cells;
    uint32_t size_cells;

    fdt_get_node_cells(node_addr, &address_cells, &size_cells);

    if (reg_prop == 0) {
        *address = 0;
        *size = 0;
        return;
    }

    if (address != (void *)0) {
        *address = address_cells == 1
            ? be32_to_cpu(*(uint32_t *)(&reg_prop->data))
            : be64_to_cpu(*(uint64_t *)(&reg_prop->data));
    }

    if (size != (void *)0) {
        uintptr_t size_addr = (uintptr_t)&reg_prop->data + address_cells * sizeof(uint32_t);

        *size = size_cells == 1
            ? be32_to_cpu(*(uint32_t *)size_addr)
            : be64_to_cpu(*(uint64_t *)size_addr);
    }
}
