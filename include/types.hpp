#pragma once

#include <openssl/sha.h>

#include <array>
#include <cstdint>
#include <string>


namespace bibochain {

typedef std::string content_t;                                // Content (data) type
typedef std::array<uint8_t, SHA256_DIGEST_LENGTH> hash_t; // Hash type
} // namespace bibochain
