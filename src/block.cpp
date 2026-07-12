#include <block.h>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
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

void Block::CalcContentHash() {
    std::vector<uint8_t> serializedData;

    for (const auto& entry : mData) {
        const auto entrySize = static_cast<uint32_t>(entry.size());

        uint8_t encodedSize[sizeof(uint32_t)];
        uint8_t* ptr = encodedSize;
        writeU32LE(ptr, entrySize);

        serializedData.insert(serializedData.end(), encodedSize,
                              encodedSize + sizeof(encodedSize));
        serializedData.insert(serializedData.end(), entry.begin(), entry.end());
    }

    SHA256(serializedData.data(), serializedData.size(),
           mHeader.mContentHash.data());
}

const hash_t& Block::GetHash() const { return mCurrentHash; }

std::string Block::GetHashStr() const {
    std::ostringstream oss;
    printHash(mCurrentHash, oss);
    return oss.str();
}

const Block* Block::GetPrevBlock() const { return mPreviousBlock; }

const hash_t& Block::GetContentHash() const { return mHeader.mContentHash; }

const hash_t& Block::GetPreviousHash() const { return mHeader.mPreviousHash; }

uint32_t Block::GetTimestamp() const { return mHeader.mTimestamp; }

uint32_t Block::GetNonce() const { return mHeader.mNonce; }

void Block::AppendData(const content_t& aData) {
    if (aData.empty()) return;

    if (mIsMined) {
        throw std::logic_error("cannot append data to a mined block");
    }

    if (aData.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::length_error("block entry is too large");
    }

    mData.push_back(aData);
    mHeader.mContentSize = static_cast<uint32_t>(mData.size());
}

const std::vector<content_t>& Block::GetData() const { return mData; }

const uint32_t& Block::GetContentSize() const { return mHeader.mContentSize; }

bool Block::IsValidHash(const hash_t& aTestHash, const hash_t& aTarget) {
    return std::memcmp(aTestHash.data(), aTarget.data(), aTestHash.size()) < 0;
}

void Block::Mine(const hash_t& aTarget) {
    if (mIsMined) {
        throw std::logic_error("block has already been mined");
    }

    CalcContentHash();

    while (true) {
        mHeader.mTimestamp = static_cast<uint32_t>(time(nullptr));

        for (uint32_t i = 0; i < UINT32_MAX; ++i) {
            mHeader.mNonce = i;

            CalcHash();

            if (IsValidHash(mCurrentHash, aTarget)) {
                mIsMined = true;
                return;
            }
        }
    }
}

} // namespace bibochain
