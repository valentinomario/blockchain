#pragma once
#include <iomanip>
#include <iostream>
#include <types.hpp>

namespace bibochain {

inline void printHash(const hash_t& aHash, std::ostream& aOutput = std::cout) {
    const auto FLAGS = aOutput.flags();
    const auto FILL = aOutput.fill();

    aOutput << "0x" << std::hex << std::setfill('0');
    for (uint8_t byte : aHash) {
        aOutput << std::setw(2) << static_cast<unsigned>(byte);
    }

    aOutput.fill(FILL);
    aOutput.flags(FLAGS);
}

static inline void writeU32LE(uint8_t*& aPointer, uint32_t aValue) {
    *aPointer++ = static_cast<uint8_t>(aValue & 0xFF);
    *aPointer++ = static_cast<uint8_t>((aValue >> 8) & 0xFF);
    *aPointer++ = static_cast<uint8_t>((aValue >> 16) & 0xFF);
    *aPointer++ = static_cast<uint8_t>((aValue >> 24) & 0xFF);
}

} // namespace bibochain
