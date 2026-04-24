#pragma once

#include <stdint.h>

#include "fdt_parser.h"

extern uintptr_t fdt_addr;

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

uint8_t fdt_setup(uintptr_t _fdt_addr);

struct fdt_header * fdt_header();

uint64_t fdt_total_size();
