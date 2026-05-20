#pragma once

#include <stddef.h>
#include <stdint.h>

inline int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen) {
    *olen = 0;
    return 0;
}
