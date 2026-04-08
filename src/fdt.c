#include "fdt.h"
#include "string.h"
#include "utils.h"

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

uint32_t fdt_check_magic(uintptr_t fdt) {
    struct fdt_header* header = (struct fdt_header*)fdt;
    return header->magic == be32_to_cpu(0xd00dfeed);
}

uint64_t fdt_total_size(uintptr_t fdt) {
    struct fdt_header* header = (struct fdt_header*)fdt;
    return be32_to_cpu(header->totalsize);
}

const char * fdt_node_name(uintptr_t node_addr) {
    return (const char *)(node_addr + TOKEN_SIZE);
}

const char * _fdt_property_name(uintptr_t fdt, struct fdt_property * property) {
    struct fdt_header* header = (struct fdt_header*)fdt;
    uint32_t nameoff = be32_to_cpu(property->nameoff);

    return (char *)((uintptr_t)fdt + be32_to_cpu(header->off_dt_strings) + nameoff);
}

uintptr_t _fdt_root_node(uintptr_t fdt) {
    struct fdt_header* header = (struct fdt_header*)fdt;
    uintptr_t start_addr = fdt + be32_to_cpu(header->off_dt_struct);

    if (*(uint32_t *)start_addr == FDT_TOKEN_BEGIN_NODE) {
        return start_addr;
    }

    return _fdt_next_token_addr(start_addr);
}

uintptr_t fdt_sibling_node(uintptr_t node_addr) {
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

/// if addr is node address, then returns first property
/// if addr is property address, then returns next property
/// if there is no property found, returns 0
uintptr_t _fdt_next_property_addr(uintptr_t addr) {
    uintptr_t property_addr = _fdt_next_token_addr(addr);

    if (*(uint32_t *)property_addr == FDT_TOKEN_PROP) {
        return property_addr;
    }

    return 0;
}

struct fdt_property * fdt_property_at_addr(uintptr_t propert_addr) {
    if (propert_addr == 0) {
        return 0;
    }

    return (struct fdt_property *)(propert_addr + TOKEN_SIZE);
}

uintptr_t fdt_node_addr_by_path(uintptr_t fdt, const char * path) {
    uintptr_t current_addr = _fdt_root_node(fdt);

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

uintptr_t fdt_property_addr_by_name(
    uintptr_t fdt, 
    uintptr_t node_addr,
    const char * property_name
) {
    if (node_addr == 0) {
        return 0;
    }

    uintptr_t current_addr = node_addr;

    while (1) {
        current_addr = _fdt_next_property_addr(current_addr);

        if (current_addr == 0) {
            return 0;
        }

        struct fdt_property * current_property = fdt_property_at_addr(current_addr);
        const char * current_property_name = _fdt_property_name(fdt, current_property);

        if (streql(current_property_name, property_name)) {
            return current_addr;
        }
    }
}

struct fdt_node_cells fdt_get_node_cells(uintptr_t fdt, uintptr_t node_addr) {
    struct fdt_node_cells result;

    uintptr_t address_cells_prop_addr = fdt_property_addr_by_name(fdt, node_addr, "#address-cells");
    uintptr_t size_cells_prop_addr = fdt_property_addr_by_name(fdt, node_addr, "#size-cells");

    struct fdt_property * address_cells_prop = fdt_property_at_addr(address_cells_prop_addr);
    struct fdt_property * size_cells_prop = fdt_property_at_addr(size_cells_prop_addr);
    
    if (address_cells_prop == 0 || size_cells_prop == 0) {
        result.error = 1;
        result.address = 0;
        result.size = 0;
        return result;
    }

    result.error = 0;
    result.address = be32_to_cpu(*(uint32_t *)(&address_cells_prop->data));
    result.size = be32_to_cpu(*(uint32_t *)(&size_cells_prop->data));

    return result;
}

void fdt_read_reg_property(
    uintptr_t fdt, uintptr_t node_addr, 
    uint32_t address_cells, uint32_t size_cells,
    uint64_t * address, uint64_t * size
) {
    uintptr_t reg_prop_addr = fdt_property_addr_by_name(fdt, node_addr, "reg");
    struct fdt_property * reg_prop = fdt_property_at_addr(reg_prop_addr);

    if (reg_prop == 0) {
        *address = 0;
        *size = 0;
        return;
    }

    *address = address_cells == 1
        ? be32_to_cpu(*(uint32_t *)(&reg_prop->data))
        : be64_to_cpu(*(uint64_t *)(&reg_prop->data));

    uintptr_t size_addr = (uintptr_t)&reg_prop->data + address_cells * sizeof(uint32_t);

    *size = size_cells == 1
        ? be32_to_cpu(*(uint32_t *)size_addr)
        : be64_to_cpu(*(uint64_t *)size_addr);
}
