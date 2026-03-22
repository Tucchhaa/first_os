#pragma once

#include <stdint.h>

uint8_t initrd_check_magic(uintptr_t file_addr);

const char * initrd_get_filepath(uintptr_t file_addr, uint32_t * path_size);

uintptr_t initrd_get_filedata(uintptr_t file_addr, uint32_t * data_size);

uintptr_t initrd_get_next_file_addr(uintptr_t file_addr);
