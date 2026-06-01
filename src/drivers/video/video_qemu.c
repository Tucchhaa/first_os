#include "video.h"

#include <stdint.h>

#include "../../kernel/mm/page_allocator.h"
#include "../../kernel/mm/utils.h"
#include "../../kernel/mm/dynamic_allocator.h"
#include "../../kernel/vmm/virtual_memory.h"
#include "../../fdt/fdt.h"
#include "../../string.h"
#include "../../uart/uart_sync.h"
#include "../../converters.h"

static uintptr_t fb_base;
static uint64_t fb_size;

static const uint32_t FB_WIDTH = 1920;
static const uint32_t FB_HEIGHT = 1080;
static const uint32_t FB_BPP = 4;
static const uint32_t XRGB8888 = 875713112;
static const uint32_t CACHE_BLOCK_SIZE = 64;

#define QEMU_PACKED __attribute__((packed))
#define bswap64(x)  __builtin_bswap64(x)
#define bswap32(x)  __builtin_bswap32(x)
#define bswap16(x)  __builtin_bswap16(x)

struct QEMU_PACKED RAMFBCfg {
    uint64_t addr;
    uint32_t fourcc;
    uint32_t flags;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

struct fw_cfg_regs {
    volatile uint16_t * select;
    volatile uint64_t * data;
    volatile uint64_t * dma;
};

static struct fw_cfg_regs regs;

static const uint32_t FW_CFG_DMA_CTL_ERROR  = 0x01;
static const uint32_t FW_CFG_DMA_CTL_READ   = 0x02;
static const uint32_t FW_CFG_DMA_CTL_SKIP   = 0x04;
static const uint32_t FW_CFG_DMA_CTL_SELECT = 0x08;
static const uint32_t FW_CFG_DMA_CTL_WRITE  = 0x10;

static const uint32_t FW_CFG_FILE_DIR = 0x19;

struct QEMU_PACKED FWCfgFile {
    uint32_t size;
    uint16_t select;
    uint16_t reserved;
    char name[56];
};

struct QEMU_PACKED FWCfgFiles {
    uint32_t count;
    struct FWCfgFile f[];
};

struct QEMU_PACKED FWCfgDmaAccess {
    uint32_t control;
    uint32_t length;
    uint64_t address;
};

static void fw_cfg_dma_transfer(
    void * address, uint32_t length, uint32_t control
) {
    volatile struct FWCfgDmaAccess access = {
        .control = bswap32(control),
        .length = bswap32(length),
        .address = bswap64(va2pa((uint64_t)address)),
    };

    *regs.dma = bswap64(va2pa((uint64_t)&access));

    while (bswap32(access.control) & ~FW_CFG_DMA_CTL_ERROR);

    asm volatile("fence" ::: "memory");
}

static void fw_cfg_read_entry(void* buf, int32_t e, int32_t len) {
    uint32_t control = (e << 16) | FW_CFG_DMA_CTL_SELECT | FW_CFG_DMA_CTL_READ;
    fw_cfg_dma_transfer(buf, len, control);
}

static void fw_cfg_write_entry(void* buf, int32_t e, int32_t len) {
    uint32_t control = (e << 16) | FW_CFG_DMA_CTL_SELECT | FW_CFG_DMA_CTL_WRITE;
    fw_cfg_dma_transfer(buf, len, control);
}

static int32_t fw_cfg_find_file(const char* name) {
    uint32_t count = 0;
    fw_cfg_read_entry(&count, FW_CFG_FILE_DIR, sizeof(count));
    count = bswap32(count);

    for (uint32_t i = 0; i < count; i++) {
        struct FWCfgFile file;
        fw_cfg_dma_transfer(&file, sizeof(file), FW_CFG_DMA_CTL_READ);

        if (streqln(name, file.name, sizeof(file.name))) {
            return bswap16(file.select);
        }
    }

    return -1;
}

#define cbo_flush(start)                            \
    ({                                              \
        uint64_t __v = (uint64_t)(start); \
        __asm__ __volatile__(                       \
            "cbo.flush"                             \
            " 0(%0)"                                \
            :                                       \
            : "rK"(__v)                             \
            : "memory");                            \
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

void video_setup() {
    uint64_t fw_cfg_base, fw_cfg_size;

    uintptr_t node = fdt_node_addr_by_compatible("qemu,fw-cfg-mmio");
    fdt_reg_property(node, &fw_cfg_base, &fw_cfg_size);

    if (fw_cfg_base == 0) {
        uart_sync_puts("[KERNEL:VIDEO]: no compatible device was found.\n");
        return;
    }

    fw_cfg_base = virtual_memory_map_mmio(fw_cfg_base, fw_cfg_size);

    regs.select = (volatile uint16_t *)(fw_cfg_base + 0x08);
    regs.data = (volatile uint64_t *)(fw_cfg_base + 0x00);
    regs.dma = (volatile uint64_t *)(fw_cfg_base + 0x10);

    fb_size = FB_HEIGHT * FB_WIDTH * FB_BPP;
    fb_base = (uintptr_t)allocate(fb_size);

    struct RAMFBCfg cfg = {
        .addr = bswap64(va2pa(fb_base)),
        .fourcc = bswap32(XRGB8888),
        .flags = bswap32(0),
        .width = bswap32(FB_WIDTH),
        .height = bswap32(FB_HEIGHT),
        .stride = bswap32(FB_WIDTH * FB_BPP),
    };

    fw_cfg_write_entry(
        &cfg, 
        fw_cfg_find_file("etc/ramfb"),
        sizeof(struct RAMFBCfg)
    );
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
