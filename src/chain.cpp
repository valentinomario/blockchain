#include <block.h>
#include <chain.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <types.hpp>
#include <vector>

namespace bibochain {

Chain::Chain(const hash_t& aDifficulty, const content_t& aGenesisData) {
    mDifficulty = aDifficulty;
    auto genesis = std::make_unique<Block>(nullptr);

    genesis->AppendData(aGenesisData);
    genesis->Mine(aDifficulty);

    mBlockChain.push_back(std::move(genesis));
    mCurrentBlockId = mBlockChain.size() - 1;
}

Chain::Chain(const hash_t& aDifficulty, const std::vector<BlockState>& aBlocks)
    : mDifficulty(aDifficulty) {
    if (aBlocks.empty()) {
        throw std::invalid_argument("cannot load an empty blockchain");
    }

    for (const auto& state : aBlocks) {
        Block* previous_block =
            mBlockChain.empty() ? nullptr : mBlockChain.back().get();
        mBlockChain.push_back(std::unique_ptr<Block>(
            new Block(previous_block, state, aDifficulty)));
    }
    mCurrentBlockId = mBlockChain.size() - 1;
}

void Chain::AppendPendingData(const content_t& aData) {
    if (!aData.empty()) {
        mPendingData.push_back(aData);
    }
}

bool Chain::MinePendingBlock() {
    if (mPendingData.empty()) {
        return false;
    }

    auto new_block =
        std::make_unique<Block>(mBlockChain[mCurrentBlockId].get());

    for (const auto& data : mPendingData) {
        new_block->AppendData(data);
    }

    new_block->Mine(mDifficulty);
    mBlockChain.push_back(std::move(new_block));
    mCurrentBlockId = mBlockChain.size() - 1;
    mPendingData.clear();
    return true;
}

const Block& Chain::GetCurrentBlock() const {
    return *mBlockChain[mCurrentBlockId];
}

} // namespace bibochain
