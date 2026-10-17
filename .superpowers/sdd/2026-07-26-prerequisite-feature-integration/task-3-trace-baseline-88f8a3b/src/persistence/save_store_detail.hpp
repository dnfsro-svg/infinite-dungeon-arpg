#pragma once

#include "persistence/checkpoint_codec.hpp"
#include "persistence/save_store.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace arpg::persistence::detail {

namespace checkpoint = arpg::dungeon::checkpoint;

enum class SlotFileState : std::uint8_t { missing, valid, invalid, unavailable };

struct SlotInfo final {
    SlotFileState state{SlotFileState::missing};
    SaveError error{SaveError::none};
    checkpoint::DungeonRunState checkpoint{};
    bool migrated{};
};

struct ScanResult final {
    SlotInfo a{};
    SlotInfo b{};
    bool temp_a{};
    bool temp_b{};
    bool any_file{};
    bool directory_error{};
    bool faulted{};
};

struct ArchiveResult final {
    bool ok{};
    SaveError error{SaveError::archive_failed};
};

struct TransactionResult final {
    bool needs_final_scan{};
    SaveCommitResult result{};
};

const std::filesystem::path& slot_name(SaveSlot slot);
const std::filesystem::path& temp_name(SaveSlot slot);
bool same_state(const checkpoint::DungeonRunState& lhs,
    const checkpoint::DungeonRunState& rhs) noexcept;
SlotInfo read_slot(const std::filesystem::path& path);
bool hook_failed(const SaveStoreConfig& config, SaveFaultPoint point) noexcept;
ScanResult scan_directory(const SaveStoreConfig& config, bool final_scan) noexcept;
SaveSlot highest_slot(const ScanResult& scan) noexcept;

void remove_temps(const SaveStoreConfig& config, ScanResult& scan) noexcept;
SaveLoadResult ready_result(const ScanResult& scan, SaveSlot slot,
    bool recovered = false) noexcept;
SaveCommitResult commit_failure(SaveCommitState state, SaveError error,
    SaveSlot slot = SaveSlot::none) noexcept;
std::vector<std::filesystem::path> invalid_files(
    const SaveStoreConfig& config, const ScanResult& scan, bool include_temps);
ArchiveResult archive_files(const SaveStoreConfig& config,
    const std::vector<std::filesystem::path>& files) noexcept;
SaveLoadResult load_recovered(const SaveStoreConfig& config) noexcept;
SaveCommitResult verify_published_transaction(const SaveStoreConfig& config,
    SaveSlot active, SaveSlot target,
    const checkpoint::DungeonRunState& expected) noexcept;

TransactionResult write_transaction(const SaveStoreConfig& config,
    SaveSlot active, const checkpoint::DungeonRunState& expected,
    const std::vector<std::uint8_t>& encoded_state) noexcept;

}  // namespace arpg::persistence::detail
