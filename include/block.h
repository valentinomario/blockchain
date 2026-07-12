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

    // Hashing stuff
    void CalcHash();

    const hash_t& GetHash() const;
    std::string GetHashStr() const;
    const Block* GetPrevBlock() const;
    const hash_t& GetContentHash() const;
    const hash_t& GetPreviousHash() const;
    uint32_t GetTimestamp() const;
    uint32_t GetNonce() const;

    // Data stuff
    void AppendData(const content_t&);
    const std::vector<content_t>& GetData() const;
    const uint32_t& GetContentSize() const;

    static bool IsValidHash(const hash_t&, const hash_t&);
    void Mine(const hash_t&);

private:
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
