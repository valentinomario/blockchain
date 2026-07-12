#pragma once

#include <storage/storage.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <types.hpp>

namespace bibochain::storage {

class LocalStorage final : public Storage {
public:
    static constexpr std::uintmax_t DEFAULT_SEGMENT_SIZE =
        std::uintmax_t{64} * 1024U * 1024U;

    explicit LocalStorage(
        std::filesystem::path aDirectory,
        std::uintmax_t aMaxSegmentSize = DEFAULT_SEGMENT_SIZE);

    std::optional<ChainState> LoadChain() override;
    void Initialize(const hash_t& aDifficulty) override;
    void AppendBlock(const BlockState& aBlock) override;

private:
    std::filesystem::path mDirectory;
    std::uintmax_t mMaxSegmentSize;
    std::optional<hash_t> mDifficulty;
};

} // namespace bibochain::storage
