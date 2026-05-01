#include "fdt.h"

#include "../converters.h"

uintptr_t fdt_addr = 0;

static uint8_t _fdt_check_magic(uintptr_t fdt) {
    struct fdt_header* header = (struct fdt_header*)fdt;
    return header->magic == be32_to_cpu(0xd00dfeed);
}

uint8_t fdt_setup(uintptr_t _fdt_addr) {
    if (_fdt_check_magic(_fdt_addr) == 0) {
        return 0;
    }

    fdt_addr = _fdt_addr;

    return 1;
}

struct fdt_header * fdt_header() {
    return (struct fdt_header *)fdt_addr;
}

uint64_t fdt_total_size() {
    struct fdt_header* header = (struct fdt_header*)fdt_addr;
    return be32_to_cpu(header->totalsize);
}
