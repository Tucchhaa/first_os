#include "string.h"

// returns 1 if strings are equal and 0 if not.
int streql(const char * a, const char * b) {
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) {
        i++;
    }

    return a[i] == b[i];
}

// buf must have size 17
void ltox(long x, char * const buf) {
    const char hex[] = "0123456789abcdef";
    const int n = 16;

    for(int i=0; i < n; i++) {
        buf[n - 1 - i] = hex[x & 0xF];
        x >>= 4;
    }

    buf[n] = '\0';
}