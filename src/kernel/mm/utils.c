#include "utils.h"

// TODO: it's possible to copy by uint64_t
void memcopy(void * dest, const void * src, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) {
        ((char *)dest)[i] = ((const char *)src)[i];
    }
}

void memzero(void * dest, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) {
        ((char *)dest)[i] = 0;
    }
}
