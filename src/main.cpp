#include <block.h>
#include <chain.h>

#include <types.hpp>
#include <utils.hpp>

using bibochain::Block;
using bibochain::Chain;
using bibochain::content_t;


static void printChainFromGenesisImpl(const Block& b, int& id) {
    if (const Block* prev = b.GetPrevBlock()) {
        printChainFromGenesisImpl(*prev, id);
    }

    std::cout << "--- Block " << id << (b.GetPrevBlock() ? "" : " (genesis)") << " ---\n";
    std::cout << "nonce: " << b.GetNonce() << "\n";
    std::cout << "hash:  " << b.GetHashStr() << "\n\n";
    ++id;
}

void printChainFromGenesis(const Block& tip) {
    int id = 0;                     // genesis sarà 0
    printChainFromGenesisImpl(tip, id);
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

    while (true) {
        std::cout << "\nInsert content (empty line to quit):\n";
        std::string content;
        if (!std::getline(std::cin, content)) break;
        if (content.empty()) break;

        chain.AppendDataToBlock(content);

        chain.NewBlock();

        const Block& tip = chain.GetCurrentBlock();

        std::cout << "New tip block mined:\n";
        std::cout << "nonce: " << tip.GetNonce() << "\n";
        std::cout << "hash:  " << tip.GetHashStr() << "\n";
    }

    if (chain.GetCurrentBlock().GetPrevBlock() == nullptr){
        std::cout<<"Empty blockchain\n";
        return 0;
    }

    std::cout<<"Printing bibochain:\n\n\n";

    auto current_block = chain.GetCurrentBlock();

    printChainFromGenesis(current_block);


    return 0;
}
