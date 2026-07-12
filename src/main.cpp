#include <block.h>
#include <chain.h>
#include <storage/local_storage.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <types.hpp>
#include <utils.hpp>
#include <vector>

using bibochain::Block;
using bibochain::Chain;
using bibochain::content_t;

namespace {

constexpr size_t DEFAULT_AUTO_COUNT = 30;
constexpr size_t DEFAULT_BATCH_SIZE = 10;

struct ProgramOptions {
    std::filesystem::path mStoragePath = "bibochain-data";
    std::uintmax_t mSegmentSize =
        bibochain::storage::LocalStorage::DEFAULT_SEGMENT_SIZE;
    bool mAutomatic = false;
    bool mShowHelp = false;
    size_t mAutoCount = DEFAULT_AUTO_COUNT;
    size_t mBatchSize = DEFAULT_BATCH_SIZE;
};

uint64_t parseUnsigned(std::string_view aValue, std::string_view aName) {
    uint64_t result = 0;
    const auto PARSE_RESULT =
        std::from_chars(aValue.data(), aValue.data() + aValue.size(), result);
    if (PARSE_RESULT.ec != std::errc{} ||
        PARSE_RESULT.ptr != aValue.data() + aValue.size()) {
        throw std::invalid_argument("invalid value for " + std::string(aName) +
                                    ": " + std::string(aValue));
    }
    return result;
}

size_t parseSize(std::string_view aValue, std::string_view aName) {
    const uint64_t VALUE = parseUnsigned(aValue, aName);
    if (VALUE > std::numeric_limits<size_t>::max()) {
        throw std::out_of_range(std::string(aName) + " is too large");
    }
    return static_cast<size_t>(VALUE);
}

ProgramOptions parseOptions(int aArgc, char* aArgv[]) {
    ProgramOptions options;
    std::vector<std::string_view> positional;
    bool automatic_option_seen = false;

    for (int index = 1; index < aArgc; ++index) {
        const std::string_view ARGUMENT = aArgv[index];
        if (ARGUMENT == "--help" || ARGUMENT == "-h") {
            options.mShowHelp = true;
        } else if (ARGUMENT == "--auto") {
            options.mAutomatic = true;
        } else if (ARGUMENT == "--auto-count") {
            if (++index >= aArgc) {
                throw std::invalid_argument("--auto-count requires a value");
            }
            options.mAutoCount = parseSize(aArgv[index], "--auto-count");
            automatic_option_seen = true;
        } else if (ARGUMENT == "--batch-size") {
            if (++index >= aArgc) {
                throw std::invalid_argument("--batch-size requires a value");
            }
            options.mBatchSize = parseSize(aArgv[index], "--batch-size");
            automatic_option_seen = true;
        } else if (ARGUMENT.rfind("--", 0) == 0) {
            throw std::invalid_argument("unknown option: " +
                                        std::string(ARGUMENT));
        } else {
            positional.push_back(ARGUMENT);
        }
    }

    if (positional.size() > 2) {
        throw std::invalid_argument("too many positional arguments");
    }
    if (!positional.empty()) {
        options.mStoragePath = positional[0];
    }
    if (positional.size() == 2) {
        const uint64_t VALUE = parseUnsigned(positional[1], "segment-size");
        if (VALUE > std::numeric_limits<std::uintmax_t>::max()) {
            throw std::out_of_range("segment-size is too large");
        }
        options.mSegmentSize = static_cast<std::uintmax_t>(VALUE);
    }
    if (automatic_option_seen && !options.mAutomatic) {
        throw std::invalid_argument(
            "--auto-count and --batch-size require --auto");
    }
    if (options.mBatchSize == 0) {
        throw std::invalid_argument("--batch-size must be greater than zero");
    }
    return options;
}

void printUsage(std::string_view aProgramName) {
    std::cout << "Usage: " << aProgramName
              << " [storage-directory] [segment-size] [options]\n\n"
                 "Options:\n"
                 "  --auto            Run in automatic mode.\n"
                 "  --auto-count N    Generate N entries (default: 30).\n"
                 "  --batch-size N    Mine every N entries (default: 10).\n"
                 "  -h, --help        Show this help.\n";
}

bool mineAndPersist(Chain& aChain, bibochain::storage::LocalStorage& aStorage) {
    if (!aChain.MinePendingBlock()) {
        return false;
    }

    const Block& tip = aChain.GetCurrentBlock();
    aStorage.AppendBlock(tip.GetState());
    std::cout << "New tip block mined:\n";
    std::cout << "nonce: " << tip.GetNonce() << "\n";
    std::cout << "hash:  " << tip.GetHashStr() << "\n";
    return true;
}

void runManualMode(Chain& aChain, bibochain::storage::LocalStorage& aStorage) {
    std::cout << "Insert content one line at a time.\n"
                 "An empty line mines the pending block; another empty line "
                 "quits.\n";

    while (true) {
        std::cout << "\nInsert content:\n";
        std::string content;
        if (!std::getline(std::cin, content)) {
            return;
        }
        if (!content.empty()) {
            aChain.AppendPendingData(content);
            continue;
        }
        if (!mineAndPersist(aChain, aStorage)) {
            return;
        }
    }
}

void runAutomaticMode(Chain& aChain, bibochain::storage::LocalStorage& aStorage,
                      size_t aEntryCount, size_t aBatchSize) {
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<uint32_t> distribution;
    size_t pending_count = 0;

    std::cout << "Automatic mode: generating " << aEntryCount
              << " entries, batch size " << aBatchSize << ".\n";
    for (size_t index = 0; index < aEntryCount; ++index) {
        const std::string CONTENT = "auto-" + std::to_string(index) + "-" +
                                    std::to_string(distribution(generator));
        std::cout << "Generated data [" << index + 1 << "/" << aEntryCount
                  << "]: " << CONTENT << '\n';
        aChain.AppendPendingData(CONTENT);
        ++pending_count;

        if (pending_count == aBatchSize) {
            if (!mineAndPersist(aChain, aStorage)) {
                throw std::logic_error("automatic batch unexpectedly empty");
            }
            pending_count = 0;
        }
    }

    if (pending_count > 0 && !mineAndPersist(aChain, aStorage)) {
        throw std::logic_error("automatic final batch unexpectedly empty");
    }
}

} // namespace

void printChainFromGenesis(const Block& aTip) {
    std::vector<const Block*> blocks;
    for (const Block* block = &aTip; block != nullptr;
         block = block->GetPrevBlock()) {
        blocks.push_back(block);
    }
    std::reverse(blocks.begin(), blocks.end());

    for (size_t id = 0; id < blocks.size(); ++id) {
        const Block& block = *blocks[id];

        std::cout << "+-- BLOCK " << id << (id == 0 ? " (GENESIS)" : "")
                  << "\n";
        std::cout << "|\n";
        std::cout << "|  HEADER\n";
        std::cout << "|    content_size:  " << block.GetContentSize() << "\n";
        std::cout << "|    content_hash:  ";
        bibochain::printHash(block.GetContentHash());
        std::cout << "\n";
        std::cout << "|    previous_hash: ";
        bibochain::printHash(block.GetPreviousHash());
        std::cout << "\n";
        std::cout << "|    timestamp:     " << block.GetTimestamp() << "\n";
        std::cout << "|    nonce:         " << block.GetNonce() << "\n";
        std::cout << "|\n";
        std::cout << "|  DATA\n";
        for (const content_t& entry : block.GetData()) {
            std::cout << "|    - " << entry << "\n";
        }
        std::cout << "|\n";
        std::cout << "|  BLOCK HASH\n";
        std::cout << "|    " << block.GetHashStr() << "\n";
        std::cout << "+--\n";

        if (id + 1 < blocks.size()) {
            const Block& next = *blocks[id + 1];
            const bool LINK_IS_VALID =
                next.GetPreviousHash() == block.GetHash();

            std::cout << "    |\n";
            std::cout << "    " << (LINK_IS_VALID ? "|" : "X") << "  Block "
                      << id + 1 << ".header.previous_hash\n";
            std::cout << "    |  == Block " << id << ".block_hash: "
                      << (LINK_IS_VALID ? "YES" : "NO - BROKEN LINK") << "\n";
            std::cout << "    v\n";
        }
    }
}

int run(int aArgc, char* aArgv[]) {
    const ProgramOptions OPTIONS = parseOptions(aArgc, aArgv);
    if (OPTIONS.mShowHelp) {
        printUsage(aArgv[0]);
        return 0;
    }

    bibochain::hash_t target{};
    target.fill(0x00);
    target[0] = 0x00;
    target[1] = 0x10;

    bibochain::storage::LocalStorage storage(OPTIONS.mStoragePath,
                                             OPTIONS.mSegmentSize);
    std::optional<bibochain::storage::ChainState> stored_chain =
        storage.LoadChain();

    if (stored_chain.has_value() && stored_chain->mDifficulty != target) {
        throw std::runtime_error(
            "stored blockchain difficulty does not match configuration");
    }

    Chain chain = stored_chain.has_value() && !stored_chain->mBlocks.empty()
                      ? Chain(target, stored_chain->mBlocks)
                      : Chain(target, "BIBO");

    if (!stored_chain.has_value()) {
        storage.Initialize(target);
    }
    if (!stored_chain.has_value() || stored_chain->mBlocks.empty()) {
        storage.AppendBlock(chain.GetCurrentBlock().GetState());
    }

    std::cout << "Using target:\n";
    bibochain::printHash(target);
    std::cout << "\nStorage directory: " << OPTIONS.mStoragePath << "\n";
    std::cout << "Maximum segment size: " << OPTIONS.mSegmentSize << " bytes\n";
    if (stored_chain.has_value() && !stored_chain->mBlocks.empty()) {
        std::cout << "Loaded " << stored_chain->mBlocks.size()
                  << " persisted blocks.\n";
    }

    if (OPTIONS.mAutomatic) {
        runAutomaticMode(chain, storage, OPTIONS.mAutoCount,
                         OPTIONS.mBatchSize);
    } else {
        runManualMode(chain, storage);
    }

    std::cout << "Printing bibochain:\n\n\n";

    printChainFromGenesis(chain.GetCurrentBlock());

    return 0;
}

int main(int aArgc, char* aArgv[]) {
    try {
        return run(aArgc, aArgv);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << "\n";
        return 1;
    }
}
