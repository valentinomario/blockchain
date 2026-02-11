#pragma once
#include <cstddef>
#include <memory>
#include <types.hpp>
#include <utils.hpp>
#include <vector>
#include <block.h>

namespace bibochain {

class Chain {
public:
    explicit Chain(const hash_t&, const content_t&);
    void AppendDataToBlock(const content_t&);
    void NewBlock();
    const Block& GetCurrentBlock() const;

private:
    std::vector<std::unique_ptr<Block>> mBlockChain;
    size_t mCurrentBlockId = 0;
    hash_t mDifficulty;

};

} // namespace bibochain