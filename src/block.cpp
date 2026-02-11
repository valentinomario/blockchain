#include <block.h>
#include <cstdint>
#include <cstring>
#include "types.hpp"

namespace bibochain {

Block::Block(Block* aBlock)
    : mPreviousBlock(aBlock),
      mHeader(aBlock != nullptr ? aBlock->GetHash() : hash_t{},
              static_cast<uint32_t>(time(nullptr))) {}

void Block::CalcHash() {
    // content size, content hash and prev hash, ts, nonce
    constexpr size_t SIZE = sizeof(uint32_t) + (2L * SHA256_DIGEST_LENGTH) +
                            sizeof(uint32_t) + sizeof(uint32_t);

    uint8_t buf[SIZE];
    uint8_t* ptr = buf;

    writeU32LE(ptr, mHeader.mContentSize);

    memcpy(ptr, mHeader.mContentHash.data(), SHA256_DIGEST_LENGTH);
    ptr += SHA256_DIGEST_LENGTH;

    memcpy(ptr, mHeader.mPreviousHash.data(), SHA256_DIGEST_LENGTH);
    ptr += SHA256_DIGEST_LENGTH;

    writeU32LE(ptr, mHeader.mTimestamp);

    writeU32LE(ptr, mHeader.mNonce);

    SHA256(buf, SIZE, mCurrentHash.data());
}

const hash_t& Block::GetHash() const { return mCurrentHash; }

std::string Block::GetHashStr() const {
    std::ostringstream oss;
    printHash(mCurrentHash, oss);
    return oss.str();
}

const Block* Block::GetPrevBlock() const { return mPreviousBlock; }

uint32_t Block::GetNonce() const { return mHeader.mNonce; }

void Block::AppendData(const content_t& aData) {
    if (aData.empty()) return;
    mData.push_back(aData);
    mHeader.mContentSize = static_cast<uint32_t>(mData.size());
}

bool Block::IsValidHash(const hash_t& aTestHash, const hash_t& aTarget) {
    return std::memcmp(aTestHash.data(), aTarget.data(), aTestHash.size()) < 0;
}

void Block::Mine(const hash_t& aTarget) {
    while (true) {
        mHeader.mTimestamp = static_cast<uint32_t>(time(nullptr));

        for (uint32_t i = 0; i < UINT32_MAX; ++i) {
            mHeader.mNonce = i;

            CalcHash();

            if (IsValidHash(mCurrentHash, aTarget)) {
                return;
            }
        }
    }
}

} // namespace bibochain