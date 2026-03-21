#include "string.h"

uint32_t kstrlen(const char * s) {
    int i = 0;

    while (s[i] != '\0') {
        i++;
    }

    return i;
}

uint8_t streqln(const char * a, const char * b, uint32_t len) {
    uint32_t i = 0;

    while (i < len && a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) {
        i++;
    }

    return i == len || a[i] == b[i];
}

uint8_t streql(const char * a, const char * b) {
    return streqln(a, b, -1);
}

uint32_t strtoken(const char * s, const char * token, uint32_t offset) {
    uint32_t tokenlen = kstrlen(token);
    uint32_t i = offset;

    while (1) {
        if (s[i] == '\0' || streqln(&s[i], token, tokenlen)) {
            return i;
        }

        i++;
    }
}

void strslice(const char * s, char * buf, uint32_t offset, uint32_t n) {
    int i = 0;

    while (i < n) {
        buf[i] = s[offset + i];
        i++;
    } 

    buf[i] = '\0';
}

void _itox(int64_t x, char * const buf, int n) {
    const char hex[] = "0123456789abcdef";

    for(int i = 0; i < n; i++) {
        buf[n - 1 - i] = hex[x & 0xF];
        x >>= 4;
    }

    buf[n] = '\0';
}

// buf must have size 17
void i64tox(int64_t x, char * const buf) { return _itox(x, buf, 16); }

// buf must have size 9
void i32tox(int32_t x, char * const buf) { return _itox(x, buf, 8); }

// buf must have size 3
void i8tox(int8_t x, char * const buf) { return _itox(x, buf, 2); }
