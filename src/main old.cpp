#include <openssl/sha.h>

#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

static volatile std::sig_atomic_t gStop = 0;

extern "C" void onSigint(int) { gStop = 1; }

using block_header_t = struct {
    // Data length in the block
    uint32_t mContentLength;

    // Hashes
    uint8_t mContentHash[32];
    uint8_t mPreviousHash[32];

    uint32_t mTimestamp;
    uint32_t mNonce;
};

// Building a history for debug
using block_t = struct {
    block_header_t mHeader;
    std::string mContent;
};

typedef std::vector<block_t> blockchain_t;

void printHash(const uint8_t* aHash) {
    auto fmt = std::cout.flags();
    auto fill = std::cout.fill();

    std::cout << "0x" << std::hex << std::setfill('0');
    for (int i = 0; i < 32; i++) {
        std::cout << std::setw(2) << static_cast<unsigned int>(aHash[i]);
    }

    std::cout.fill(fill);
    std::cout.flags(fmt);
}
void printBlockchain(const blockchain_t& aBlockchain) {
    std::cout << "\n=== BIBOCHAIN (" << aBlockchain.size() << " blocks) ===\n";
    for (std::size_t i = 0; i < aBlockchain.size(); ++i) {
        const auto& b = aBlockchain[i];
        std::cout << "\n--- Block " << i << " ---\n";
        std::cout << "content: " << b.mContent << "\n";
        std::cout << "contentLen: " << b.mHeader.mContentLength << "\n";
        std::cout << "timestamp: " << b.mHeader.mTimestamp << "\n";
        std::cout << "nonce: " << b.mHeader.mNonce << "\n";

        std::cout << "contentHash: ";
        printHash(b.mHeader.mContentHash);
        std::cout << "\n";

        std::cout << "prevHash:    ";
        printHash(b.mHeader.mPreviousHash);
        std::cout << "\n";
    }
    std::cout << "\n=== END ===\n";
}

block_header_t buildBlock(const block_header_t* aPreviousBlock,
                          const void* aContent, uint64_t aLength) {
    block_header_t o_header{};

    // Hashes

    // TODO(improvements): serialize the block before computing hash
    if (aPreviousBlock != nullptr) {
        SHA256(reinterpret_cast<const unsigned char*>(aPreviousBlock),
               sizeof(*aPreviousBlock), o_header.mPreviousHash);
    } else {
        memset(o_header.mPreviousHash, 0, sizeof(o_header.mPreviousHash));
    }

    // Content

    SHA256(reinterpret_cast<const unsigned char*>(aContent), aLength,
           o_header.mContentHash);
    o_header.mContentLength = aLength;
    return o_header;
}

void mineBlock(block_header_t* aHeader, const uint8_t* aTarget) {
    uint8_t block_hash[32];

    while (gStop == 0) {
        aHeader->mTimestamp = static_cast<uint32_t>(time(0));

        for (int i = 0; i < UINT32_MAX && gStop == 0; i++) {
            aHeader->mNonce = i;

            SHA256(reinterpret_cast<const unsigned char*>(aHeader),
                   sizeof(block_header_t), block_hash);

            if (memcmp(block_hash, aTarget, sizeof(block_hash)) < 0) return;
        }
        std::cout<<"Searching timestamp\n";
    }
}

int main() {
    std::signal(SIGINT, onSigint);

    uint8_t target[32] = {};
    target[2] = 0x5;
    blockchain_t blockchain;

    std::cout << "Using target hash:\n";
    printHash(target);
    std::cout << "\n";

    std::cout << "Generating first block:\n";
    int block_num = 0;
    block_header_t previous_header = {};

    while (gStop == 0) {
        std::cout << "Input content:\n";
        std::string content;

        if (!std::getline(std::cin, content)) {
            std::cout << "Exiting\n";
            break;
        }

        block_header_t* previous_header_ptr =
            block_num > 0 ? &previous_header : nullptr;
        auto content_length = content.length();
        std::cout << "Creating block " << block_num << " with content len "
                  << content_length << "\n";

        auto new_block_header =
            buildBlock(previous_header_ptr, content.data(), content_length);

        mineBlock(&new_block_header, target);
        previous_header = new_block_header;
        block_num++;

        std::cout << "Mined new block: timestamp "
                  << new_block_header.mTimestamp << " nonce "
                  << new_block_header.mNonce << "\n";

        // Saving history for debug
        blockchain.emplace_back(block_t{new_block_header, content});
    }
    printBlockchain(blockchain);
    return 0;
}
