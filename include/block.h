#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <types.hpp>
#include <vector>

namespace bibochain {

using block_header_t = struct BlockHeaderType {
    // Data length in the block
    uint32_t mContentSize{};

    // Hashes
    hash_t mContentHash{};
    hash_t mPreviousHash{};

    uint32_t mTimestamp;
    uint32_t mNonce{};
    BlockHeaderType() = delete;
    BlockHeaderType(const hash_t& aPrevHash, const uint32_t& aTimestamp)
        : mPreviousHash(aPrevHash), mTimestamp(aTimestamp) {};
};

struct BlockState {
    uint32_t mContentSize{};
    hash_t mContentHash{};
    hash_t mPreviousHash{};
    uint32_t mTimestamp{};
    uint32_t mNonce{};
    hash_t mBlockHash{};
    std::vector<content_t> mData;
};

class Block {
public:
    explicit Block(Block* aBlock);

    // Hashing stuff
    void CalcHash();

    const hash_t& GetHash() const;
    std::string GetHashStr() const;
    const Block* GetPrevBlock() const;
    const hash_t& GetContentHash() const;
    const hash_t& GetPreviousHash() const;
    uint32_t GetTimestamp() const;
    uint32_t GetNonce() const;
    BlockState GetState() const;

    // Data stuff
    void AppendData(const content_t& aData);
    const std::vector<content_t>& GetData() const;
    const uint32_t& GetContentSize() const;

    static bool IsValidHash(const hash_t& aTestHash, const hash_t& aTarget);
    void Mine(const hash_t& aTarget);

private:
    friend class Chain;

    Block(Block* aBlock, const BlockState& aState, const hash_t& aDifficulty);
    void CalcContentHash();

    // Block content
    block_header_t mHeader;
    std::vector<content_t> mData;

    // Management
    Block* mPreviousBlock = nullptr;
    hash_t mCurrentHash{};
    bool mIsMined = false;
};

} // namespace bibochain
