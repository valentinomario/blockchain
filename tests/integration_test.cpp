#include <block.h>
#include <chain.h>
#include <storage/local_storage.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <types.hpp>
#include <vector>

namespace {

constexpr std::uintmax_t TEST_SEGMENT_SIZE = 250;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto TIMESTAMP =
            std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path BASE =
            std::filesystem::temp_directory_path();

        for (uint32_t index = 0; index < 100; ++index) {
            mPath =
                BASE / ("bibochain-integration-" + std::to_string(TIMESTAMP) +
                        "-" + std::to_string(index));
            if (std::filesystem::create_directory(mPath)) {
                return;
            }
        }
        throw std::runtime_error("cannot create temporary test directory");
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(mPath, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    const std::filesystem::path& GetPath() const { return mPath; }

private:
    std::filesystem::path mPath;
};

void require(bool aCondition, std::string_view aMessage) {
    if (!aCondition) {
        throw std::runtime_error(std::string(aMessage));
    }
}

std::filesystem::path segmentPath(const std::filesystem::path& aDirectory,
                                  uint32_t aIndex) {
    std::string index = std::to_string(aIndex);
    index.insert(0, 5U - index.size(), '0');
    return aDirectory / ("blocks-" + index + ".dat");
}

void appendMinedBlock(bibochain::Chain& aChain,
                      bibochain::storage::LocalStorage& aStorage) {
    require(aChain.MinePendingBlock(), "pending block was not mined");
    aStorage.AppendBlock(aChain.GetCurrentBlock().GetState());
}

void verifyLoadedContents(const bibochain::Chain& aChain) {
    const bibochain::Block* block = &aChain.GetCurrentBlock();
    require(block->GetData() == std::vector<bibochain::content_t>{"gamma"},
            "tip content differs after reload");

    block = block->GetPrevBlock();
    require(block != nullptr, "missing first mined block");
    require(
        block->GetData() == std::vector<bibochain::content_t>{"alpha", "beta"},
        "multi-content block differs after reload");

    block = block->GetPrevBlock();
    require(block != nullptr, "missing genesis block");
    require(block->GetData() == std::vector<bibochain::content_t>{"BIBO"},
            "genesis content differs after reload");
    require(block->GetPrevBlock() == nullptr,
            "genesis unexpectedly references another block");
}

void runIntegrationTest() {
    TemporaryDirectory directory;

    bibochain::hash_t target{};
    target[0] = 0x00;
    target[1] = 0x10;

    bibochain::storage::LocalStorage storage(directory.GetPath(),
                                             TEST_SEGMENT_SIZE);
    storage.Initialize(target);

    bibochain::Chain chain(target, "BIBO");
    storage.AppendBlock(chain.GetCurrentBlock().GetState());

    chain.AppendPendingData("alpha");
    chain.AppendPendingData("beta");
    appendMinedBlock(chain, storage);

    chain.AppendPendingData("gamma");
    appendMinedBlock(chain, storage);

    require(std::filesystem::exists(segmentPath(directory.GetPath(), 0)),
            "genesis segment was not created");
    require(std::filesystem::exists(segmentPath(directory.GetPath(), 1)),
            "first segment rotation did not occur");
    require(std::filesystem::exists(segmentPath(directory.GetPath(), 2)),
            "second segment rotation did not occur");
    require(!std::filesystem::exists(segmentPath(directory.GetPath(), 3)),
            "an unexpected segment was created");

    bibochain::storage::LocalStorage loader(directory.GetPath(),
                                            TEST_SEGMENT_SIZE);
    const std::optional<bibochain::storage::ChainState> LOADED =
        loader.LoadChain();
    if (!LOADED.has_value()) {
        throw std::runtime_error("stored chain was not found");
    }
    const bibochain::storage::ChainState& loaded = *LOADED;
    require(loaded.mDifficulty == target, "stored target differs");
    require(loaded.mBlocks.size() == 3, "wrong loaded block count");

    bibochain::Chain restored(target, loaded.mBlocks);
    verifyLoadedContents(restored);

    restored.AppendPendingData("delta");
    appendMinedBlock(restored, loader);
    require(std::filesystem::exists(segmentPath(directory.GetPath(), 3)),
            "append after reload did not rotate the segment");

    bibochain::storage::LocalStorage final_loader(directory.GetPath(),
                                                  TEST_SEGMENT_SIZE);
    const auto FINAL_STATE = final_loader.LoadChain();
    if (!FINAL_STATE.has_value()) {
        throw std::runtime_error("chain disappeared after append");
    }
    const bibochain::storage::ChainState& final_state = *FINAL_STATE;
    require(final_state.mBlocks.size() == 4,
            "append after reload was not persisted");

    const std::filesystem::path LAST_SEGMENT =
        segmentPath(directory.GetPath(), 3);
    const std::uintmax_t VALID_SIZE = std::filesystem::file_size(LAST_SEGMENT);
    {
        std::ofstream output(LAST_SEGMENT, std::ios::binary | std::ios::app);
        const std::array<char, 2> PARTIAL_RECORD = {'\x01', '\x02'};
        output.write(PARTIAL_RECORD.data(), PARTIAL_RECORD.size());
        require(static_cast<bool>(output),
                "cannot simulate an interrupted write");
    }

    bibochain::storage::LocalStorage recovery_loader(directory.GetPath(),
                                                     TEST_SEGMENT_SIZE);
    const auto RECOVERED_STATE = recovery_loader.LoadChain();
    if (!RECOVERED_STATE.has_value()) {
        throw std::runtime_error("recovery lost the chain");
    }
    const bibochain::storage::ChainState& recovered_state = *RECOVERED_STATE;
    require(recovered_state.mBlocks.size() == 4, "recovery lost a valid block");
    require(std::filesystem::file_size(LAST_SEGMENT) == VALID_SIZE,
            "partial record was not removed during recovery");

    bibochain::Chain recovered(target, recovered_state.mBlocks);
    require(recovered.GetCurrentBlock().GetData() ==
                std::vector<bibochain::content_t>{"delta"},
            "recovered tip content differs");
}

} // namespace

int main() {
    try {
        runIntegrationTest();
        std::cout << "bibochain integration test passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "bibochain integration test failed: " << exception.what()
                  << '\n';
        return 1;
    }
}
