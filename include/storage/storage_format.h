#pragma once

#include <block.h>
#include <openssl/sha.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <types.hpp>
#include <vector>

namespace bibochain::storage::format {

inline constexpr std::array<char, 8> SEGMENT_MAGIC = {'B', 'I', 'B', 'O',
                                                      'S', 'E', 'G', '\0'};
inline constexpr uint32_t FORMAT_VERSION = 1;
inline constexpr size_t SEGMENT_HEADER_SIZE =
    SEGMENT_MAGIC.size() + sizeof(uint32_t) + sizeof(uint32_t) +
    SHA256_DIGEST_LENGTH;
inline constexpr uint32_t MAX_RECORD_SIZE = 64U * 1024U * 1024U;
inline constexpr uint32_t MAX_CONTENT_COUNT = 1'000'000U;
inline constexpr std::string_view SEGMENT_PREFIX = "blocks-";
inline constexpr std::string_view SEGMENT_SUFFIX = ".dat";

struct SegmentInfo {
    uint32_t mIndex;
    std::filesystem::path mPath;
};

struct SegmentHeaderResult {
    bool mComplete = false;
    bool mMagicValid = false;
    uint32_t mVersion = 0;
    uint32_t mIndex = 0;
    hash_t mDifficulty{};
};

enum class RecordStatus : uint8_t {
    Complete,
    End,
    PartialLength,
    PartialRecord,
    InvalidSize
};

struct RecordResult {
    RecordStatus mStatus = RecordStatus::End;
    size_t mOffset = 0;
    size_t mNextOffset = 0;
    uint32_t mPayloadSize = 0;
    size_t mAvailableBytes = 0;
    std::vector<uint8_t> mPayload;
    hash_t mStoredChecksum{};
    hash_t mCalculatedChecksum{};
    bool mChecksumPresent = false;
    bool mChecksumValid = false;
    std::optional<BlockState> mBlock;
    std::string mDecodeError;
};

std::optional<uint32_t> parseSegmentIndex(const std::filesystem::path& aPath);
std::filesystem::path segmentPath(const std::filesystem::path& aDirectory,
                                  uint32_t aIndex);
std::vector<SegmentInfo> listSegments(
    const std::filesystem::path& aDirectory,
    std::vector<std::filesystem::path>* aIgnoredFiles = nullptr);
std::vector<uint8_t> readFile(const std::filesystem::path& aPath);

std::vector<uint8_t> encodeSegmentHeader(uint32_t aIndex,
                                         const hash_t& aDifficulty);
SegmentHeaderResult decodeSegmentHeader(const std::vector<uint8_t>& aData);

std::vector<uint8_t> encodeBlock(const BlockState& aBlock);
BlockState decodeBlock(const std::vector<uint8_t>& aPayload);
std::vector<uint8_t> encodeRecord(const BlockState& aBlock);
RecordResult decodeRecord(const std::vector<uint8_t>& aData, size_t aOffset);

hash_t calculateHash(const std::vector<uint8_t>& aData);
hash_t calculateContentHash(const BlockState& aBlock);
hash_t calculateBlockHash(const BlockState& aBlock);
bool satisfiesTarget(const hash_t& aHash, const hash_t& aTarget);

} // namespace bibochain::storage::format
