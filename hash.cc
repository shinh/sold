#include "hash.h"

uint32_t CalcGnuHash(const std::string& name) {
    uint32_t h = 5381;
    for (unsigned char c : name) {
        h = h * 33 + c;
    }
    return h;
}

uint32_t CalcHash(const std::string& name) {
    uint32_t h = 0, g;
    for (unsigned char c : name) {
        h = (h << 4) + c;
        g = h & 0xf0000000;
        h ^= g >> 24;
        h ^= g;
    }
    return h;
}
