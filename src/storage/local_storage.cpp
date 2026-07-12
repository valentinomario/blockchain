#include <block.h>
#include <storage/local_storage.h>
#include <storage/storage_format.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <types.hpp>
#include <utility>
#include <vector>

namespace bibochain::storage {

namespace {

void writeFile(const std::filesystem::path& aPath,
               const std::vector<uint8_t>& aData, std::ios::openmode aMode) {
    std::ofstream output(aPath, std::ios::binary | aMode);
    if (!output) {
        throw std::runtime_error("cannot open storage segment: " +
                                 aPath.string());
    }
    output.write(reinterpret_cast<const char*>(aData.data()),
                 static_cast<std::streamsize>(aData.size()));
    output.flush();
    if (!output) {
        throw std::runtime_error("cannot write storage segment: " +
                                 aPath.string());
    }
}

void recoverTruncatedTail(const std::filesystem::path& aPath,
                          bool aIsLastSegment, std::uintmax_t aRecordStart) {
    if (!aIsLastSegment) {
        throw std::runtime_error("truncated non-final segment");
    }
    std::filesystem::resize_file(aPath, aRecordStart);
}

void validateSegmentHeader(const format::SegmentHeaderResult& aHeader,
                           const format::SegmentInfo& aSegment) {
    if (!aHeader.mComplete) {
        throw std::runtime_error("truncated storage segment header");
    }
    if (!aHeader.mMagicValid) {
        throw std::runtime_error("invalid storage segment magic");
    }
    if (aHeader.mVersion != format::FORMAT_VERSION) {
        throw std::runtime_error("unsupported storage format version");
    }
    if (aHeader.mIndex != aSegment.mIndex) {
        throw std::runtime_error("segment index does not match filename");
    }
}

bool isPartial(format::RecordStatus aStatus) {
    return aStatus == format::RecordStatus::PartialLength ||
           aStatus == format::RecordStatus::PartialRecord;
}

} // namespace

LocalStorage::LocalStorage(std::filesystem::path aDirectory,
                           std::uintmax_t aMaxSegmentSize)
    : mDirectory(std::move(aDirectory)), mMaxSegmentSize(aMaxSegmentSize) {
    if (mMaxSegmentSize <=
        format::SEGMENT_HEADER_SIZE + sizeof(uint32_t) + SHA256_DIGEST_LENGTH) {
        throw std::invalid_argument("segment size limit is too small");
    }
}

std::optional<ChainState> LocalStorage::LoadChain() {
    const std::vector<format::SegmentInfo> SEGMENTS =
        format::listSegments(mDirectory);
    if (SEGMENTS.empty()) {
        return std::nullopt;
    }

    ChainState chain{};
    for (size_t position = 0; position < SEGMENTS.size(); ++position) {
        const format::SegmentInfo& segment = SEGMENTS[position];
        if (position > std::numeric_limits<uint32_t>::max() ||
            segment.mIndex != static_cast<uint32_t>(position)) {
            throw std::runtime_error("missing or duplicated storage segment");
        }

        const std::vector<uint8_t> DATA = format::readFile(segment.mPath);
        const format::SegmentHeaderResult HEADER =
            format::decodeSegmentHeader(DATA);
        validateSegmentHeader(HEADER, segment);
        if (position == 0) {
            chain.mDifficulty = HEADER.mDifficulty;
            mDifficulty = HEADER.mDifficulty;
        } else if (HEADER.mDifficulty != chain.mDifficulty) {
            throw std::runtime_error("difficulty differs between segments");
        }

        const bool IS_LAST_SEGMENT = position + 1 == SEGMENTS.size();
        size_t offset = format::SEGMENT_HEADER_SIZE;
        while (true) {
            const format::RecordResult RECORD =
                format::decodeRecord(DATA, offset);
            if (RECORD.mStatus == format::RecordStatus::End) {
                break;
            }
            if (isPartial(RECORD.mStatus)) {
                recoverTruncatedTail(segment.mPath, IS_LAST_SEGMENT,
                                     RECORD.mOffset);
                break;
            }
            if (RECORD.mStatus == format::RecordStatus::InvalidSize) {
                throw std::runtime_error("invalid block record size");
            }
            if (!RECORD.mChecksumValid) {
                throw std::runtime_error("invalid block record checksum");
            }
            if (!RECORD.mBlock.has_value()) {
                throw std::runtime_error("invalid block payload: " +
                                         RECORD.mDecodeError);
            }
            chain.mBlocks.push_back(*RECORD.mBlock);
            offset = RECORD.mNextOffset;
        }
    }
    return chain;
}

void LocalStorage::Initialize(const hash_t& aDifficulty) {
    if (!format::listSegments(mDirectory).empty()) {
        throw std::logic_error("storage has already been initialized");
    }

    std::filesystem::create_directories(mDirectory);
    writeFile(format::segmentPath(mDirectory, 0),
              format::encodeSegmentHeader(0, aDifficulty), std::ios::trunc);
    mDifficulty = aDifficulty;
}

void LocalStorage::AppendBlock(const BlockState& aBlock) {
    if (!mDifficulty.has_value()) {
        throw std::logic_error("storage is not initialized");
    }

    const std::vector<uint8_t> RECORD = format::encodeRecord(aBlock);
    if (format::SEGMENT_HEADER_SIZE + RECORD.size() > mMaxSegmentSize) {
        throw std::length_error("block does not fit in a storage segment");
    }

    std::vector<format::SegmentInfo> segments =
        format::listSegments(mDirectory);
    if (segments.empty()) {
        throw std::logic_error("storage has no initial segment");
    }

    format::SegmentInfo current = segments.back();
    const std::uintmax_t CURRENT_SIZE =
        std::filesystem::file_size(current.mPath);
    if (CURRENT_SIZE + RECORD.size() > mMaxSegmentSize) {
        if (current.mIndex == std::numeric_limits<uint32_t>::max()) {
            throw std::overflow_error("too many storage segments");
        }
        ++current.mIndex;
        current.mPath = format::segmentPath(mDirectory, current.mIndex);
        writeFile(current.mPath,
                  format::encodeSegmentHeader(current.mIndex, *mDifficulty),
                  std::ios::trunc);
    }

    writeFile(current.mPath, RECORD, std::ios::app);
}

} // namespace bibochain::storage
