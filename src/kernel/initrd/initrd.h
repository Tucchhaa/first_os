#pragma once

#include <stdint.h>

#include "initrd_parser.h"

extern uintptr_t initrd_start_addr;
extern uintptr_t initrd_end_addr;

uint8_t initrd_setup();
