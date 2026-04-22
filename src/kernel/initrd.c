#include "initrd.h"

#include "../string.h"

struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

inline uint8_t initrd_check_magic(uintptr_t file_addr) {
    return streqln((const char *)file_addr, "070701", 6);
}

const char * initrd_get_filepath(uintptr_t file_addr, uint32_t * path_size) {
    if (initrd_check_magic(file_addr) == 0) {
        return 0;
    }

    struct cpio_newc_header * header = (struct cpio_newc_header *)file_addr;
    *path_size = xtoi32(header->c_namesize);

    return (const char *)(file_addr + sizeof(struct cpio_newc_header));
}

uintptr_t initrd_get_filedata(uintptr_t file_addr, uint32_t * file_size) {
    if (initrd_check_magic(file_addr) == 0) {
        return 0;
    }

    struct cpio_newc_header * header = (struct cpio_newc_header *)file_addr;
    uint32_t name_size = xtoi32(header->c_namesize);

    *file_size = xtoi32(header->c_filesize);

    uintptr_t addr = file_addr + sizeof(struct cpio_newc_header) + name_size;
    addr = (addr + 3) & ~3;

    return addr;
}

uintptr_t initrd_get_next_file_addr(uintptr_t file_addr) {
    if (initrd_check_magic(file_addr) == 0) {
        return 0;
    }

    struct cpio_newc_header * header = (struct cpio_newc_header *)file_addr;

    uint32_t path_size = xtoi32(header->c_namesize);
    uint32_t file_size = xtoi32(header->c_filesize);

    uintptr_t next_addr = file_addr + sizeof(struct cpio_newc_header) + path_size;
    next_addr = (next_addr + 3) & ~3;
    next_addr += file_size;
    next_addr = (next_addr + 3) & ~3;

    uint32_t next_path_size;
    const char * next_name = initrd_get_filepath(next_addr, &next_path_size);

    if (next_name == 0 || streqln(next_name, "TRAILER!!!", next_path_size)) {
        return 0;
    }

    return next_addr;
}

uintptr_t initrd_get_file_addr(uintptr_t initrd_start_addr, const char * filepath) {
    if (initrd_check_magic(initrd_start_addr) == 0) {
        return 0;
    }

    uintptr_t file_addr = initrd_start_addr;

    while (file_addr != 0) {
        uint32_t path_size;
        const char * current_filepath = initrd_get_filepath(file_addr, &path_size);

        if (streqln(filepath, current_filepath, path_size)) {
            return file_addr;
        }

        file_addr = initrd_get_next_file_addr(file_addr);
    }

    return 0;
}
