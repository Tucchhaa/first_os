#include "initrd.h"

#include "../../fdt/fdt.h"
#include "../../converters.h"

uintptr_t initrd_start_addr;
uintptr_t initrd_end_addr;

uint8_t initrd_setup() {
    uintptr_t chosen_node = fdt_node_addr_by_path("/chosen");
    struct fdt_property * initrd_start_prop = fdt_property_by_name(chosen_node, "linux,initrd-start");
    struct fdt_property * initrd_end_prop = fdt_property_by_name(chosen_node, "linux,initrd-end");

    if (initrd_start_prop == 0 || initrd_end_prop == 0) {
        return 0;
    }

    initrd_start_addr = be64_to_cpu(*(uint64_t *)(&initrd_start_prop->data));
    initrd_end_addr = be64_to_cpu(*(uint64_t *)(&initrd_end_prop->data));

    if (initrd_check_magic(initrd_start_addr) == 0) {
        return 0;
    }

    return 1;
}
