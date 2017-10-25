#pragma once
#include <immintrin.h>
#include <stdint.h>
#include "common.h"

const uint64_t x_mask = 0x5555555555555555;
const uint64_t y_mask = 0xAAAAAAAAAAAAAAAA;

struct vector morton_decode(const uint64_t m) {
    uint32_t x = _pext_u64(m, x_mask);
    uint32_t y = _pext_u64(m, y_mask);
    return (struct vector){
        .x = x,
        .y = y,
        .z = 0,
    };
}

uint64_t morton_encode(struct vector v) {
    uint32_t x = v.x;
    uint32_t y = v.y;
    return
        _pdep_u64(x, x_mask) |
        _pdep_u64(y, y_mask);
}

/*const uint64_t x_mask = 0x1249249249249249;
const uint64_t y_mask = 0x2492492492492492;
const uint64_t z_mask = 0x4924924924924924;

struct vector morton_decode(const uint64_t m) {
    uint32_t x = _pext_u64(m, x_mask);
    uint32_t y = _pext_u64(m, y_mask);
    uint32_t z = _pext_u64(m, z_mask);
    return (struct vector){
        .x = x,
        .y = y,
        .z = z,
    };
};

uint64_t morton_encode(struct vector v) {
    uint32_t x = v.x;
    uint32_t y = v.y;
    uint32_t z = v.z;
    return
        _pdep_u64(x, x_mask) |
        _pdep_u64(y, y_mask) |
        _pdep_u64(z, z_mask);
};*/
