#pragma once

#include <stdint.h>

void video_setup();

void video_bmp_display(uint32_t * bmp_image, uint32_t width, uint32_t height);
