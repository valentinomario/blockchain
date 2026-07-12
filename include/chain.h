#pragma once
#include <block.h>

#include <cstddef>
#include <memory>
#include <types.hpp>
#include <utils.hpp>
#include <vector>

namespace bibochain {

class Chain {
public:
    explicit Chain(const hash_t& aDifficulty, const content_t& aGenesisData);
    Chain(const hash_t& aDifficulty, const std::vector<BlockState>& aBlocks);
    void AppendPendingData(const content_t& aData);
    bool MinePendingBlock();
    const Block& GetCurrentBlock() const;

private:
    std::vector<std::unique_ptr<Block>> mBlockChain;
    std::vector<content_t> mPendingData;
    size_t mCurrentBlockId = 0;
    hash_t mDifficulty;
};

} // namespace bibochain
