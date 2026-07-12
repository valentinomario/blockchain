#pragma once

#include <block.h>

#include <optional>
#include <types.hpp>
#include <vector>

namespace bibochain::storage {

struct ChainState {
    hash_t mDifficulty{};
    std::vector<BlockState> mBlocks;
};

class Storage {
public:
    virtual ~Storage() = default;

    virtual std::optional<ChainState> LoadChain() = 0;
    virtual void Initialize(const hash_t& aDifficulty) = 0;
    virtual void AppendBlock(const BlockState& aBlock) = 0;
};

} // namespace bibochain::storage
