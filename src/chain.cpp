#include <block.h>
#include <chain.h>

#include <algorithm>
#include <memory>
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
