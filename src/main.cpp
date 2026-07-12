#include <block.h>
#include <chain.h>

#include <algorithm>
#include <types.hpp>
#include <utils.hpp>
#include <vector>

using bibochain::Block;
using bibochain::Chain;
using bibochain::content_t;

void printChainFromGenesis(const Block& tip) {
    std::vector<const Block*> blocks;
    for (const Block* block = &tip; block != nullptr;
         block = block->GetPrevBlock()) {
        blocks.push_back(block);
    }
    std::reverse(blocks.begin(), blocks.end());

    for (size_t id = 0; id < blocks.size(); ++id) {
        const Block& block = *blocks[id];

        std::cout << "+-- BLOCK " << id
                  << (id == 0 ? " (GENESIS)" : "") << "\n";
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
            const bool linkIsValid = next.GetPreviousHash() == block.GetHash();

            std::cout << "    |\n";
            std::cout << "    |  Block " << id + 1
                      << ".header.previous_hash\n";
            std::cout << "    |  == Block " << id << ".block_hash: "
                      << (linkIsValid ? "YES" : "NO - BROKEN LINK") << "\n";
            std::cout << "    v\n";
        }
    }
}

int main() {
    bibochain::hash_t target{};
    target.fill(0x00);
    target[0] = 0x00;
    target[1] = 0x10;

    std::cout << "Using target:\n";
    bibochain::printHash(target);
    std::cout << "\n";

    Chain chain(target, "BIBO");

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
        std::cout << "New tip block mined:\n";
        std::cout << "nonce: " << tip.GetNonce() << "\n";
        std::cout << "hash:  " << tip.GetHashStr() << "\n";
    }

    std::cout << "Printing bibochain:\n\n\n";

    printChainFromGenesis(chain.GetCurrentBlock());

    return 0;
}
