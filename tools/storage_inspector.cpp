#include <block.h>
#include <storage/storage_format.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <types.hpp>
#include <utils.hpp>
#include <vector>

namespace {

namespace storage_format = bibochain::storage::format;

struct InspectionState {
    size_t mSegments = 0;
    size_t mRecords = 0;
    size_t mValidRecords = 0;
    size_t mPartialRecords = 0;
    size_t mIssues = 0;
    std::optional<bibochain::hash_t> mDifficulty;
    std::optional<bibochain::hash_t> mPreviousBlockHash;
};

std::string hashString(const bibochain::hash_t& aHash) {
    std::ostringstream output;
    bibochain::printHash(aHash, output);
    return output.str();
}

std::string formatTimestamp(uint32_t aTimestamp) {
    const std::time_t RAW_TIMESTAMP = static_cast<std::time_t>(aTimestamp);
    const std::tm* utc_pointer = std::gmtime(&RAW_TIMESTAMP);
    if (utc_pointer == nullptr) {
        return "invalid timestamp";
    }

    std::ostringstream output;
    output << std::put_time(utc_pointer, "%Y-%m-%d %H:%M:%S UTC");
    return output.str();
}

std::string escapeContent(std::string_view aContent) {
    constexpr size_t MAX_DISPLAY_SIZE = 160;
    std::ostringstream output;
    const size_t DISPLAY_SIZE = std::min(aContent.size(), MAX_DISPLAY_SIZE);
    for (size_t index = 0; index < DISPLAY_SIZE; ++index) {
        const unsigned char CHARACTER =
            static_cast<unsigned char>(aContent[index]);
        switch (CHARACTER) {
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '"':
                output << "\\\"";
                break;
            default:
                if (std::isprint(CHARACTER) != 0) {
                    output << static_cast<char>(CHARACTER);
                } else {
                    output << "\\x" << std::hex << std::setw(2)
                           << std::setfill('0')
                           << static_cast<unsigned>(CHARACTER) << std::dec;
                }
        }
    }
    if (aContent.size() > DISPLAY_SIZE) {
        output << "... (" << aContent.size() - DISPLAY_SIZE << " more bytes)";
    }
    return output.str();
}

void printCheck(std::string_view aLabel, bool aSuccess,
                std::string_view aFailure, bool& aRecordValid,
                InspectionState& aState) {
    std::cout << "|  |  " << std::left << std::setw(22) << aLabel << ": "
              << std::right << (aSuccess ? "[OK]" : "[ERROR]") << '\n';
    if (!aSuccess) {
        std::cout << "|  |    reason: " << aFailure << '\n';
        aRecordValid = false;
        ++aState.mIssues;
    }
}

void inspectCompleteRecord(const storage_format::RecordResult& aRecord,
                           InspectionState& aState) {
    bool record_valid = true;
    printCheck("record checksum", aRecord.mChecksumValid,
               "stored checksum differs from SHA-256(payload)", record_valid,
               aState);
    std::cout << "|  |    stored:     " << hashString(aRecord.mStoredChecksum)
              << '\n';
    std::cout << "|  |    calculated: "
              << hashString(aRecord.mCalculatedChecksum) << '\n';

    if (!aRecord.mBlock.has_value()) {
        std::cout << "|  |  payload decode        : [ERROR]\n";
        std::cout << "|  |    reason: " << aRecord.mDecodeError << '\n';
        std::cout << "|  |  STATUS: [CORRUPT]\n";
        ++aState.mIssues;
        return;
    }

    const bibochain::BlockState& block = *aRecord.mBlock;
    const size_t BLOCK_HEIGHT = aState.mRecords;
    std::cout << "|  |\n";
    std::cout << "|  |  BLOCK " << BLOCK_HEIGHT
              << (BLOCK_HEIGHT == 0 ? " (GENESIS)" : "") << '\n';
    std::cout << "|  |    record_offset:  " << aRecord.mOffset << " (0x"
              << std::hex << aRecord.mOffset << std::dec << ")\n";
    std::cout << "|  |    content_count:  " << block.mContentSize << '\n';
    std::cout << "|  |    content_hash:   " << hashString(block.mContentHash)
              << '\n';
    std::cout << "|  |    previous_hash:  " << hashString(block.mPreviousHash)
              << '\n';
    std::cout << "|  |    timestamp:      " << block.mTimestamp << " ("
              << formatTimestamp(block.mTimestamp) << ")\n";
    std::cout << "|  |    nonce:          " << block.mNonce << '\n';
    std::cout << "|  |    block_hash:     " << hashString(block.mBlockHash)
              << '\n';

    const bibochain::hash_t CALCULATED_CONTENT_HASH =
        storage_format::calculateContentHash(block);
    const bibochain::hash_t CALCULATED_BLOCK_HASH =
        storage_format::calculateBlockHash(block);
    printCheck("content hash", CALCULATED_CONTENT_HASH == block.mContentHash,
               "content data does not match content_hash", record_valid,
               aState);
    printCheck("block hash", CALCULATED_BLOCK_HASH == block.mBlockHash,
               "serialized header does not match block_hash", record_valid,
               aState);
    printCheck("proof of work",
               aState.mDifficulty.has_value() &&
                   storage_format::satisfiesTarget(block.mBlockHash,
                                                   *aState.mDifficulty),
               "block hash does not satisfy segment difficulty", record_valid,
               aState);

    bibochain::hash_t expected_previous_hash{};
    if (aState.mPreviousBlockHash.has_value()) {
        expected_previous_hash = *aState.mPreviousBlockHash;
    }
    printCheck("previous link", block.mPreviousHash == expected_previous_hash,
               BLOCK_HEIGHT == 0
                   ? "genesis previous_hash is not zero"
                   : "previous_hash differs from preceding block_hash",
               record_valid, aState);

    std::cout << "|  |  DATA\n";
    for (size_t index = 0; index < block.mData.size(); ++index) {
        std::cout << "|  |    [" << index << "] " << block.mData[index].size()
                  << " bytes: \"" << escapeContent(block.mData[index])
                  << "\"\n";
    }
    if (block.mData.empty()) {
        std::cout << "|  |    (empty)\n";
    }

    std::cout << "|  |  STATUS: " << (record_valid ? "[VALID]" : "[CORRUPT]")
              << '\n';
    if (record_valid) {
        ++aState.mValidRecords;
    }
    aState.mPreviousBlockHash = block.mBlockHash;
}

void printPartialRecord(const storage_format::RecordResult& aRecord,
                        bool aIsLastSegment, InspectionState& aState) {
    const size_t EXPECTED_BYTES =
        aRecord.mStatus == storage_format::RecordStatus::PartialLength
            ? sizeof(uint32_t)
            : sizeof(uint32_t) + static_cast<size_t>(aRecord.mPayloadSize) +
                  SHA256_DIGEST_LENGTH;
    const char* kind =
        aRecord.mStatus == storage_format::RecordStatus::PartialLength
            ? "PARTIAL LENGTH PREFIX"
            : "PARTIAL RECORD";
    std::cout << "|  +-- " << kind << " @ " << aRecord.mOffset << " (0x"
              << std::hex << aRecord.mOffset << std::dec << ")\n";
    if (aRecord.mStatus == storage_format::RecordStatus::PartialRecord) {
        std::cout << "|  |  declared payload: " << aRecord.mPayloadSize
                  << " bytes\n";
    }
    std::cout << "|  |  available:        " << aRecord.mAvailableBytes << " / "
              << EXPECTED_BYTES << " record bytes\n";
    std::cout << "|  |  status:           [PARTIAL] "
              << (aIsLastSegment ? "recoverable tail"
                                 : "corruption in non-final segment")
              << '\n';
    std::cout << "|  +--\n";
    ++aState.mPartialRecords;
    ++aState.mIssues;
}

void inspectRecords(const std::vector<uint8_t>& aData, bool aIsLastSegment,
                    InspectionState& aState) {
    size_t offset = storage_format::SEGMENT_HEADER_SIZE;
    size_t segment_record = 0;
    while (true) {
        const storage_format::RecordResult RECORD =
            storage_format::decodeRecord(aData, offset);
        if (RECORD.mStatus == storage_format::RecordStatus::End) {
            break;
        }
        if (RECORD.mStatus == storage_format::RecordStatus::PartialLength ||
            RECORD.mStatus == storage_format::RecordStatus::PartialRecord) {
            printPartialRecord(RECORD, aIsLastSegment, aState);
            return;
        }
        if (RECORD.mStatus == storage_format::RecordStatus::InvalidSize) {
            std::cout << "|  +-- RECORD " << segment_record << " @ "
                      << RECORD.mOffset << '\n';
            std::cout << "|  |  declared payload: " << RECORD.mPayloadSize
                      << " bytes\n";
            std::cout << "|  |  status: [ERROR] invalid record size; cannot "
                         "locate next record\n";
            std::cout << "|  +--\n";
            ++aState.mIssues;
            return;
        }

        std::cout << "|  +-- RECORD " << segment_record << " / GLOBAL BLOCK "
                  << aState.mRecords << '\n';
        std::cout << "|  |  payload_size: " << RECORD.mPayloadSize
                  << " bytes\n";
        inspectCompleteRecord(RECORD, aState);
        std::cout << "|  +--\n";
        offset = RECORD.mNextOffset;
        ++segment_record;
        ++aState.mRecords;
    }

    if (segment_record == 0) {
        std::cout << "|  (no block records)\n";
    }
}

void inspectSegment(const storage_format::SegmentInfo& aSegment,
                    bool aIsLastSegment, InspectionState& aState) {
    const std::vector<uint8_t> DATA = storage_format::readFile(aSegment.mPath);
    const storage_format::SegmentHeaderResult HEADER =
        storage_format::decodeSegmentHeader(DATA);
    ++aState.mSegments;

    std::cout
        << "+============================================================\n";
    std::cout << "| SEGMENT FILE: " << aSegment.mPath.filename() << '\n';
    std::cout << "| filename index: " << aSegment.mIndex << '\n';
    std::cout << "| file size:      " << DATA.size() << " bytes\n";
    std::cout << "| role:           "
              << (aIsLastSegment ? "active/final segment" : "closed segment")
              << '\n';

    if (!HEADER.mComplete) {
        std::cout << "| HEADER: [PARTIAL] " << DATA.size() << " / "
                  << storage_format::SEGMENT_HEADER_SIZE << " bytes\n";
        std::cout << "+========================================================"
                     "====\n";
        ++aState.mIssues;
        ++aState.mPartialRecords;
        return;
    }

    std::cout << "| HEADER\n";
    std::cout << "|  magic:          "
              << (HEADER.mMagicValid ? "[OK]" : "[ERROR]") << '\n';
    std::cout << "|  format version: " << HEADER.mVersion
              << (HEADER.mVersion == storage_format::FORMAT_VERSION
                      ? " [OK]"
                      : " [UNSUPPORTED]")
              << '\n';
    std::cout << "|  declared index: " << HEADER.mIndex
              << (HEADER.mIndex == aSegment.mIndex ? " [OK]" : " [MISMATCH]")
              << '\n';
    std::cout << "|  difficulty:     " << hashString(HEADER.mDifficulty)
              << '\n';

    bool header_valid = HEADER.mMagicValid &&
                        HEADER.mVersion == storage_format::FORMAT_VERSION &&
                        HEADER.mIndex == aSegment.mIndex;
    if (!header_valid) {
        ++aState.mIssues;
    }
    if (!aState.mDifficulty.has_value()) {
        aState.mDifficulty = HEADER.mDifficulty;
    } else if (*aState.mDifficulty != HEADER.mDifficulty) {
        std::cout << "|  difficulty consistency: [ERROR] differs from first "
                     "segment\n";
        header_valid = false;
        ++aState.mIssues;
    } else {
        std::cout << "|  difficulty consistency: [OK]\n";
    }

    if (!header_valid) {
        std::cout << "| RECORD PARSING: skipped because header is invalid\n";
    } else {
        std::cout << "| RECORDS\n";
        inspectRecords(DATA, aIsLastSegment, aState);
    }
    std::cout
        << "+============================================================\n";
}

int inspectStorage(const std::filesystem::path& aDirectory) {
    if (!std::filesystem::exists(aDirectory)) {
        throw std::runtime_error("storage directory does not exist");
    }
    if (!std::filesystem::is_directory(aDirectory)) {
        throw std::runtime_error("storage path is not a directory");
    }

    std::vector<std::filesystem::path> ignored_files;
    const std::vector<storage_format::SegmentInfo> SEGMENTS =
        storage_format::listSegments(aDirectory, &ignored_files);

    std::cout << "BIBOCHAIN LOCAL STORAGE INSPECTOR\n";
    std::cout << "Directory: " << std::filesystem::absolute(aDirectory) << '\n';
    std::cout << "Mode:      read-only (no recovery or file modification)\n";
    std::cout
        << "Note:      max segment size is not persisted in format v1\n\n";

    if (!ignored_files.empty()) {
        std::cout << "IGNORED NON-SEGMENT FILES\n";
        for (const auto& path : ignored_files) {
            std::cout << "  - " << path.filename() << '\n';
        }
        std::cout << '\n';
    }

    InspectionState state{};
    if (SEGMENTS.empty()) {
        std::cout << "[ERROR] No blocks-*.dat segment files found.\n";
        return 1;
    }

    for (size_t position = 0; position < SEGMENTS.size(); ++position) {
        const storage_format::SegmentInfo& segment = SEGMENTS[position];
        if (position > std::numeric_limits<uint32_t>::max() ||
            segment.mIndex != static_cast<uint32_t>(position)) {
            std::cout << "[ERROR] Expected segment index " << position
                      << " but found " << segment.mIndex << " ("
                      << segment.mPath.filename() << ")\n";
            ++state.mIssues;
        }
        inspectSegment(segment, position + 1 == SEGMENTS.size(), state);
        std::cout << '\n';
    }

    std::cout
        << "+========================== SUMMARY =========================\n";
    std::cout << "| segment files:   " << state.mSegments << '\n';
    std::cout << "| complete blocks: " << state.mRecords << '\n';
    std::cout << "| valid blocks:    " << state.mValidRecords << '\n';
    std::cout << "| partial records: " << state.mPartialRecords << '\n';
    std::cout << "| issues:          " << state.mIssues << '\n';
    std::cout << "| overall status: "
              << (state.mIssues == 0 ? "[HEALTHY]" : "[ISSUES FOUND]") << '\n';
    std::cout
        << "+============================================================\n";
    return state.mIssues == 0 ? 0 : 1;
}

} // namespace

int main(int aArgc, char* aArgv[]) {
    if (aArgc != 2) {
        std::cerr << "Usage: " << aArgv[0] << " <local-storage-directory>\n";
        return 2;
    }

    try {
        return inspectStorage(aArgv[1]);
    } catch (const std::exception& exception) {
        std::cerr << "Inspector error: " << exception.what() << '\n';
        return 2;
    }
}
