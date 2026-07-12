#include <block.h>

#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <types.hpp>
#include <utils.hpp>
#include <vector>

namespace bibochain {

Block::Block(Block* aBlock)
    : mPreviousBlock(aBlock),
      mHeader(aBlock != nullptr ? aBlock->GetHash() : hash_t{},
              static_cast<uint32_t>(time(nullptr))) {}

Block::Block(Block* aBlock, const BlockState& aState, const hash_t& aDifficulty)
    : mHeader(aState.mPreviousHash, aState.mTimestamp),
      mData(aState.mData),
      mPreviousBlock(aBlock) {
    const hash_t EXPECTED_PREVIOUS_HASH =
        aBlock == nullptr ? hash_t{} : aBlock->GetHash();
    if (aState.mPreviousHash != EXPECTED_PREVIOUS_HASH) {
        throw std::invalid_argument("broken previous block link");
    }
    if (aState.mContentSize != aState.mData.size()) {
        throw std::invalid_argument("invalid block content count");
    }
    for (const auto& entry : mData) {
        if (entry.empty()) {
            throw std::invalid_argument("empty block content entry");
        }
    }

    mHeader.mContentSize = aState.mContentSize;
    mHeader.mContentHash = aState.mContentHash;
    mHeader.mNonce = aState.mNonce;

    CalcContentHash();
    if (mHeader.mContentHash != aState.mContentHash) {
        throw std::invalid_argument("invalid block content hash");
    }

    CalcHash();
    if (mCurrentHash != aState.mBlockHash) {
        throw std::invalid_argument("invalid stored block hash");
    }
    if (!IsValidHash(mCurrentHash, aDifficulty)) {
        throw std::invalid_argument("stored block does not satisfy target");
    }
    mIsMined = true;
}

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
    std::vector<uint8_t> serialized_data;

    for (const auto& entry : mData) {
        const auto ENTRY_SIZE = static_cast<uint32_t>(entry.size());

        uint8_t encoded_size[sizeof(uint32_t)];
        uint8_t* ptr = encoded_size;
        writeU32LE(ptr, ENTRY_SIZE);

        serialized_data.insert(serialized_data.end(), encoded_size,
                               encoded_size + sizeof(encoded_size));
        serialized_data.insert(serialized_data.end(), entry.begin(),
                               entry.end());
    }

    SHA256(serialized_data.data(), serialized_data.size(),
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

BlockState Block::GetState() const {
    return BlockState{mHeader.mContentSize,
                      mHeader.mContentHash,
                      mHeader.mPreviousHash,
                      mHeader.mTimestamp,
                      mHeader.mNonce,
                      mCurrentHash,
                      mData};
}

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
