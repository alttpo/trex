#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline uint32_t ld8(uint8_t **p) {
    uint32_t a = *(*p)++;
    return a;
}

static inline uint32_t ld16(uint8_t **p) {
    uint32_t a = *(*p)++;
    a |= (uint32_t)(*(*p)++) << 8;
    return a;
}

static inline uint32_t ld24(uint8_t **p) {
    uint32_t a = *(*p)++;
    a |= (uint32_t)(*(*p)++) << 8;
    a |= (uint32_t)(*(*p)++) << 16;
    return a;
}

static inline uint32_t ld32(uint8_t **p) {
    uint32_t a = *(*p)++;
    a |= (uint32_t)(*(*p)++) << 8;
    a |= (uint32_t)(*(*p)++) << 16;
    a |= (uint32_t)(*(*p)++) << 24;
    return a;
}

#ifdef __cplusplus
}
#endif
