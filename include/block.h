#pragma once

#include <cstdint>
#include <string>
#include <types.hpp>
#include <utils.hpp>
#include <vector>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <ctime>


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

class Block {
public:
    explicit Block(Block*);

    void CalcHash();

    const hash_t& GetHash() const;
    std::string GetHashStr() const;
    const Block* GetPrevBlock() const;
    uint32_t GetNonce() const;

    void AppendData(const content_t&);

    static bool IsValidHash(const hash_t&, const hash_t&);
    void Mine(const hash_t&);

private:
    // Block content
    block_header_t mHeader;
    std::vector<content_t> mData;

    // Management
    Block* mPreviousBlock = nullptr;
    hash_t mCurrentHash{};
};

} // namespace bibochain