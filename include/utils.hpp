#pragma once
#include <iomanip>
#include <iostream>
#include <types.hpp>

namespace bibochain {

inline void printHash(const hash_t& h, std::ostream& os = std::cout) {
    const auto fmt = os.flags();
    const auto fill = os.fill();

    os << "0x" << std::hex << std::setfill('0');
    for (uint8_t b : h) {
        os << std::setw(2) << static_cast<unsigned>(b);
    }

    os.fill(fill);
    os.flags(fmt);
}

static inline void writeU32LE(uint8_t*& p, uint32_t v) {
    *p++ = static_cast<uint8_t>( v        & 0xFF);
    *p++ = static_cast<uint8_t>((v >> 8)  & 0xFF);
    *p++ = static_cast<uint8_t>((v >> 16) & 0xFF);
    *p++ = static_cast<uint8_t>((v >> 24) & 0xFF);
}


} // namespace bibochain