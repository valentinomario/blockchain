#include <block.h>
#include <chain.h>
#include <storage/local_storage.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <types.hpp>
#include <utils.hpp>
#include <vector>

using bibochain::Block;
using bibochain::Chain;
using bibochain::content_t;

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
    bibochain::hash_t target{};
    target.fill(0x00);
    target[0] = 0x00;
    target[1] = 0x10;

    const std::filesystem::path STORAGE_PATH =
        aArgc > 1 ? aArgv[1] : "bibochain-data";
    const std::uintmax_t SEGMENT_SIZE =
        aArgc > 2 ? static_cast<std::uintmax_t>(std::stoull(aArgv[2]))
                  : bibochain::storage::LocalStorage::DEFAULT_SEGMENT_SIZE;
    bibochain::storage::LocalStorage storage(STORAGE_PATH, SEGMENT_SIZE);
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
    std::cout << "\nStorage directory: " << STORAGE_PATH << "\n";
    std::cout << "Maximum segment size: " << SEGMENT_SIZE << " bytes\n";
    if (stored_chain.has_value() && !stored_chain->mBlocks.empty()) {
        std::cout << "Loaded " << stored_chain->mBlocks.size()
                  << " persisted blocks.\n";
    }

    std::cout << "Insert content one line at a time.\n"
                 "An empty line mines the pending block; another empty line "
                 "quits.\n";

    while (true) {
        std::cout << "\nInsert content:\n";
        std::string content;
        if (!std::getline(std::cin, content)) break;

        if (!content.empty()) {
            chain.AppendPendingData(content);
            continue;
        }

        if (!chain.MinePendingBlock()) {
            break;
        }

        const Block& tip = chain.GetCurrentBlock();
        storage.AppendBlock(tip.GetState());
        std::cout << "New tip block mined:\n";
        std::cout << "nonce: " << tip.GetNonce() << "\n";
        std::cout << "hash:  " << tip.GetHashStr() << "\n";
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
