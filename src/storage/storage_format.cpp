#include <block.h>
#include <openssl/sha.h>
#include <storage/storage_format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <types.hpp>
#include <utility>
#include <vector>

namespace bibochain::storage::format {

namespace {

class PayloadReader {
public:
    explicit PayloadReader(const std::vector<uint8_t>& aPayload)
        : mPayload(aPayload) {}

    uint32_t ReadU32() {
        Require(sizeof(uint32_t), "truncated uint32 in block payload");
        const uint32_t VALUE =
            static_cast<uint32_t>(mPayload[mOffset]) |
            (static_cast<uint32_t>(mPayload[mOffset + 1]) << 8U) |
            (static_cast<uint32_t>(mPayload[mOffset + 2]) << 16U) |
            (static_cast<uint32_t>(mPayload[mOffset + 3]) << 24U);
        mOffset += sizeof(uint32_t);
        return VALUE;
    }

    hash_t ReadHash() {
        Require(SHA256_DIGEST_LENGTH, "truncated hash in block payload");
        hash_t result{};
        std::copy_n(mPayload.begin() + static_cast<std::ptrdiff_t>(mOffset),
                    SHA256_DIGEST_LENGTH, result.begin());
        mOffset += SHA256_DIGEST_LENGTH;
        return result;
    }

    std::string ReadString(uint32_t aSize) {
        Require(aSize, "truncated content in block payload");
        const auto* start =
            reinterpret_cast<const char*>(mPayload.data() + mOffset);
        std::string result(start, aSize);
        mOffset += aSize;
        return result;
    }

    size_t Remaining() const { return mPayload.size() - mOffset; }

private:
    void Require(size_t aSize, std::string_view aMessage) const {
        if (aSize > mPayload.size() - mOffset) {
            throw std::runtime_error(std::string(aMessage));
        }
    }

    const std::vector<uint8_t>& mPayload;
    size_t mOffset = 0;
};

void appendU32(std::vector<uint8_t>& aOutput, uint32_t aValue) {
    aOutput.push_back(static_cast<uint8_t>(aValue & 0xFFU));
    aOutput.push_back(static_cast<uint8_t>((aValue >> 8U) & 0xFFU));
    aOutput.push_back(static_cast<uint8_t>((aValue >> 16U) & 0xFFU));
    aOutput.push_back(static_cast<uint8_t>((aValue >> 24U) & 0xFFU));
}

void appendHash(std::vector<uint8_t>& aOutput, const hash_t& aHash) {
    aOutput.insert(aOutput.end(), aHash.begin(), aHash.end());
}

uint32_t readU32At(const std::vector<uint8_t>& aData, size_t aOffset) {
    return static_cast<uint32_t>(aData[aOffset]) |
           (static_cast<uint32_t>(aData[aOffset + 1]) << 8U) |
           (static_cast<uint32_t>(aData[aOffset + 2]) << 16U) |
           (static_cast<uint32_t>(aData[aOffset + 3]) << 24U);
}

hash_t readHashAt(const std::vector<uint8_t>& aData, size_t aOffset) {
    hash_t result{};
    std::copy_n(aData.begin() + static_cast<std::ptrdiff_t>(aOffset),
                SHA256_DIGEST_LENGTH, result.begin());
    return result;
}

} // namespace

std::optional<uint32_t> parseSegmentIndex(const std::filesystem::path& aPath) {
    const std::string NAME = aPath.filename().string();
    if (NAME.size() <= SEGMENT_PREFIX.size() + SEGMENT_SUFFIX.size() ||
        NAME.compare(0, SEGMENT_PREFIX.size(), SEGMENT_PREFIX) != 0 ||
        NAME.compare(NAME.size() - SEGMENT_SUFFIX.size(), SEGMENT_SUFFIX.size(),
                     SEGMENT_SUFFIX) != 0) {
        return std::nullopt;
    }

    const std::string DIGITS =
        NAME.substr(SEGMENT_PREFIX.size(), NAME.size() - SEGMENT_PREFIX.size() -
                                               SEGMENT_SUFFIX.size());
    if (!std::all_of(DIGITS.begin(), DIGITS.end(), [](char aCharacter) {
            return std::isdigit(static_cast<unsigned char>(aCharacter)) != 0;
        })) {
        return std::nullopt;
    }

    const uint64_t VALUE = std::stoull(DIGITS);
    if (VALUE > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("segment index is too large: " + NAME);
    }
    return static_cast<uint32_t>(VALUE);
}

std::filesystem::path segmentPath(const std::filesystem::path& aDirectory,
                                  uint32_t aIndex) {
    std::ostringstream name;
    name << SEGMENT_PREFIX << std::setfill('0') << std::setw(5) << aIndex
         << SEGMENT_SUFFIX;
    return aDirectory / name.str();
}

std::vector<SegmentInfo> listSegments(
    const std::filesystem::path& aDirectory,
    std::vector<std::filesystem::path>* aIgnoredFiles) {
    if (!std::filesystem::exists(aDirectory)) {
        return {};
    }
    if (!std::filesystem::is_directory(aDirectory)) {
        throw std::runtime_error("storage path is not a directory");
    }

    std::vector<SegmentInfo> segments;
    for (const auto& entry : std::filesystem::directory_iterator(aDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto INDEX = parseSegmentIndex(entry.path());
        if (INDEX.has_value()) {
            segments.push_back(SegmentInfo{*INDEX, entry.path()});
        } else if (aIgnoredFiles != nullptr) {
            aIgnoredFiles->push_back(entry.path());
        }
    }
    std::sort(segments.begin(), segments.end(),
              [](const SegmentInfo& aLeft, const SegmentInfo& aRight) {
                  return aLeft.mIndex < aRight.mIndex;
              });
    if (aIgnoredFiles != nullptr) {
        std::sort(aIgnoredFiles->begin(), aIgnoredFiles->end());
    }
    return segments;
}

std::vector<uint8_t> readFile(const std::filesystem::path& aPath) {
    const std::uintmax_t FILE_SIZE = std::filesystem::file_size(aPath);
    if (FILE_SIZE > std::numeric_limits<size_t>::max()) {
        throw std::runtime_error("segment is too large to read");
    }

    std::vector<uint8_t> data(static_cast<size_t>(FILE_SIZE));
    std::ifstream input(aPath, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open segment: " + aPath.string());
    }
    input.read(reinterpret_cast<char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    if (input.gcount() != static_cast<std::streamsize>(data.size())) {
        throw std::runtime_error("cannot read complete segment: " +
                                 aPath.string());
    }
    return data;
}

std::vector<uint8_t> encodeSegmentHeader(uint32_t aIndex,
                                         const hash_t& aDifficulty) {
    std::vector<uint8_t> result;
    result.insert(result.end(), SEGMENT_MAGIC.begin(), SEGMENT_MAGIC.end());
    appendU32(result, FORMAT_VERSION);
    appendU32(result, aIndex);
    appendHash(result, aDifficulty);
    return result;
}

SegmentHeaderResult decodeSegmentHeader(const std::vector<uint8_t>& aData) {
    SegmentHeaderResult result{};
    if (aData.size() < SEGMENT_HEADER_SIZE) {
        return result;
    }

    result.mComplete = true;
    result.mMagicValid =
        std::equal(SEGMENT_MAGIC.begin(), SEGMENT_MAGIC.end(), aData.begin());
    result.mVersion = readU32At(aData, SEGMENT_MAGIC.size());
    result.mIndex = readU32At(aData, SEGMENT_MAGIC.size() + sizeof(uint32_t));
    result.mDifficulty =
        readHashAt(aData, SEGMENT_MAGIC.size() + (2U * sizeof(uint32_t)));
    return result;
}

std::vector<uint8_t> encodeBlock(const BlockState& aBlock) {
    if (aBlock.mContentSize != aBlock.mData.size()) {
        throw std::invalid_argument("block state has an invalid content count");
    }

    std::vector<uint8_t> payload;
    appendU32(payload, aBlock.mContentSize);
    appendHash(payload, aBlock.mContentHash);
    appendHash(payload, aBlock.mPreviousHash);
    appendU32(payload, aBlock.mTimestamp);
    appendU32(payload, aBlock.mNonce);
    appendHash(payload, aBlock.mBlockHash);
    for (const auto& entry : aBlock.mData) {
        if (entry.size() > std::numeric_limits<uint32_t>::max()) {
            throw std::length_error("block entry is too large to store");
        }
        appendU32(payload, static_cast<uint32_t>(entry.size()));
        payload.insert(payload.end(), entry.begin(), entry.end());
    }
    return payload;
}

BlockState decodeBlock(const std::vector<uint8_t>& aPayload) {
    PayloadReader reader(aPayload);
    BlockState block{};
    block.mContentSize = reader.ReadU32();
    if (block.mContentSize > MAX_CONTENT_COUNT) {
        throw std::runtime_error("content count exceeds safety limit");
    }
    block.mContentHash = reader.ReadHash();
    block.mPreviousHash = reader.ReadHash();
    block.mTimestamp = reader.ReadU32();
    block.mNonce = reader.ReadU32();
    block.mBlockHash = reader.ReadHash();
    block.mData.reserve(block.mContentSize);
    for (uint32_t index = 0; index < block.mContentSize; ++index) {
        block.mData.push_back(reader.ReadString(reader.ReadU32()));
    }
    if (reader.Remaining() != 0) {
        throw std::runtime_error("unexpected bytes after block payload");
    }
    return block;
}

std::vector<uint8_t> encodeRecord(const BlockState& aBlock) {
    const std::vector<uint8_t> PAYLOAD = encodeBlock(aBlock);
    if (PAYLOAD.empty() || PAYLOAD.size() > MAX_RECORD_SIZE ||
        PAYLOAD.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::length_error("block record is too large");
    }

    const hash_t CHECKSUM = calculateHash(PAYLOAD);
    std::vector<uint8_t> result;
    result.reserve(sizeof(uint32_t) + PAYLOAD.size() + CHECKSUM.size());
    appendU32(result, static_cast<uint32_t>(PAYLOAD.size()));
    result.insert(result.end(), PAYLOAD.begin(), PAYLOAD.end());
    appendHash(result, CHECKSUM);
    return result;
}

RecordResult decodeRecord(const std::vector<uint8_t>& aData, size_t aOffset) {
    RecordResult result{};
    result.mOffset = aOffset;
    if (aOffset == aData.size()) {
        result.mStatus = RecordStatus::End;
        return result;
    }
    if (aOffset > aData.size()) {
        result.mStatus = RecordStatus::InvalidSize;
        return result;
    }

    result.mAvailableBytes = aData.size() - aOffset;
    if (result.mAvailableBytes < sizeof(uint32_t)) {
        result.mStatus = RecordStatus::PartialLength;
        return result;
    }

    result.mPayloadSize = readU32At(aData, aOffset);
    if (result.mPayloadSize == 0 || result.mPayloadSize > MAX_RECORD_SIZE) {
        result.mStatus = RecordStatus::InvalidSize;
        return result;
    }

    const size_t TOTAL_SIZE = sizeof(uint32_t) +
                              static_cast<size_t>(result.mPayloadSize) +
                              SHA256_DIGEST_LENGTH;
    if (result.mAvailableBytes < TOTAL_SIZE) {
        result.mStatus = RecordStatus::PartialRecord;
        return result;
    }

    const size_t PAYLOAD_OFFSET = aOffset + sizeof(uint32_t);
    const auto PAYLOAD_BEGIN =
        aData.begin() + static_cast<std::ptrdiff_t>(PAYLOAD_OFFSET);
    result.mPayload.assign(
        PAYLOAD_BEGIN,
        PAYLOAD_BEGIN + static_cast<std::ptrdiff_t>(result.mPayloadSize));
    const size_t CHECKSUM_OFFSET = PAYLOAD_OFFSET + result.mPayloadSize;
    result.mStoredChecksum = readHashAt(aData, CHECKSUM_OFFSET);
    result.mCalculatedChecksum = calculateHash(result.mPayload);
    result.mChecksumPresent = true;
    result.mChecksumValid =
        result.mStoredChecksum == result.mCalculatedChecksum;
    result.mNextOffset = aOffset + TOTAL_SIZE;
    result.mStatus = RecordStatus::Complete;

    try {
        result.mBlock = decodeBlock(result.mPayload);
    } catch (const std::exception& exception) {
        result.mDecodeError = exception.what();
    }
    return result;
}

hash_t calculateHash(const std::vector<uint8_t>& aData) {
    hash_t result{};
    SHA256(aData.data(), aData.size(), result.data());
    return result;
}

hash_t calculateContentHash(const BlockState& aBlock) {
    std::vector<uint8_t> serialized_data;
    for (const auto& entry : aBlock.mData) {
        appendU32(serialized_data, static_cast<uint32_t>(entry.size()));
        serialized_data.insert(serialized_data.end(), entry.begin(),
                               entry.end());
    }
    return calculateHash(serialized_data);
}

hash_t calculateBlockHash(const BlockState& aBlock) {
    std::vector<uint8_t> header;
    appendU32(header, aBlock.mContentSize);
    appendHash(header, aBlock.mContentHash);
    appendHash(header, aBlock.mPreviousHash);
    appendU32(header, aBlock.mTimestamp);
    appendU32(header, aBlock.mNonce);
    return calculateHash(header);
}

bool satisfiesTarget(const hash_t& aHash, const hash_t& aTarget) {
    return std::memcmp(aHash.data(), aTarget.data(), aHash.size()) < 0;
}

} // namespace bibochain::storage::format
