#include "video.h"

#include "../../kernel/mm/utils.h"

static uintptr_t fb_base;

static const uint32_t FB_WIDTH = 1920;
static const uint32_t FB_HEIGHT = 1080;
static const uint32_t CACHE_BLOCK_SIZE = 64;

void video_setup() {
    fb_base = 0x7f700000;
}

#define cbo_flush(start)                \
    ({                                  \
        asm volatile("mv a0, %0\n\t"    \
                     ".word 0x0025200F" \
                     :                  \
                     : "r"(start)       \
                     : "memory", "a0"); \
    })

static void flush_dcache(void * addr, uint64_t len) {
    uint64_t start = (uint64_t)addr & ~(CACHE_BLOCK_SIZE - 1);
    __sync_synchronize();

    for (
        uint64_t line = start; 
        line < (uint64_t)addr + len;
        line += CACHE_BLOCK_SIZE
    ) {
        cbo_flush(line);
        __sync_synchronize();
    }
}

void video_bmp_display(uint32_t * bmp_image, uint32_t width, uint32_t height) {
    uint32_t * fb = (uint32_t *)fb_base;
    uint32_t start_x = (FB_WIDTH - width) / 2;
    uint32_t start_y = (FB_HEIGHT - height) / 2;

    for (uint32_t y = 0; y < height; y++) {
        void * dst = fb + (start_y + y) * FB_WIDTH + start_x;

        memcopy(dst, bmp_image + y * width, width * sizeof(uint32_t));
        flush_dcache(dst, width * sizeof(uint32_t));
    }
}
