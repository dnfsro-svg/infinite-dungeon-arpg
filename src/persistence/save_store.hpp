#pragma once

#include "dungeon/dungeon_checkpoint.hpp"

#include <cstdint>
#include <filesystem>

namespace arpg::persistence {

enum class SaveSlot : std::uint8_t { none, a, b };

enum class SaveCommitState : std::uint8_t {
    committed,
    not_committed,
    indeterminate
};

enum class SaveLoadState : std::uint8_t {
    ready,
    empty,
    recovery_required,
    blocked
};

enum class SaveError : std::uint8_t {
    none,
    directory_unavailable,
    read_failed,
    write_failed,
    flush_failed,
    publish_failed,
    final_scan_failed,
    conflicting_slots,
    archive_failed,
    invalid_checkpoint
};

enum class SaveFaultPoint : std::uint8_t {
    before_temp_write,
    after_temp_write,
    after_temp_validation,
    before_publish,
    after_publish,
    final_scan_a,
    final_scan_b,
    before_archive
};

using SaveFaultHook = bool (*)(SaveFaultPoint point, void* context) noexcept;
using UtcStampProvider = std::uint64_t (*)(void* context) noexcept;

struct SaveStoreConfig final {
    std::filesystem::path directory{};
    SaveFaultHook fault_hook{};
    void* fault_context{};
    UtcStampProvider stamp_provider{};
    void* stamp_context{};
};

struct SaveLoadResult final {
    SaveLoadState state{SaveLoadState::blocked};
    SaveError error{SaveError::none};
    SaveSlot active_slot{SaveSlot::none};
    bool recovered{};
    bool migrated{};
    dungeon::checkpoint::DungeonRunState checkpoint{};
};

struct SaveCommitResult final {
    SaveCommitState state{SaveCommitState::indeterminate};
    SaveError error{SaveError::none};
    SaveSlot active_slot{SaveSlot::none};
    dungeon::checkpoint::DungeonRunState verified_state{};
};

class SaveStore final {
public:
    explicit SaveStore(SaveStoreConfig config);

    [[nodiscard]] SaveLoadResult load() noexcept;

    [[nodiscard]] SaveCommitResult commit(
        const dungeon::checkpoint::DungeonRunState& expected) noexcept;

    [[nodiscard]] SaveLoadResult archive_invalid_and_create(
        const dungeon::checkpoint::DungeonRunState& initial) noexcept;

private:
    SaveStoreConfig config_{};
};

}  // namespace arpg::persistence
