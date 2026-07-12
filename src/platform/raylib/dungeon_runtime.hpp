#pragma once

#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_view_math.hpp"
#include "persistence/save_store.hpp"

#include <cstdint>
#include <optional>

namespace arpg::platform {

enum class DungeonRuntimeState : std::uint8_t {
    uninitialized,
    running,
    recovery_required,
    faulted,
};

using RootSeedProvider = std::optional<std::uint64_t> (*)(void* context) noexcept;

struct DungeonRuntimeConfig final {
    dungeon::DungeonRules rules{};
    persistence::SaveStoreConfig save{};
    std::optional<std::uint64_t> new_run_seed{};
    RootSeedProvider seed_provider{};
    void* seed_context{};
};

struct DungeonRenderStatus final {
    SaveIndicator indicator{SaveIndicator::none};
    persistence::SaveSlot active_slot{persistence::SaveSlot::none};
    persistence::SaveError error{persistence::SaveError::none};
};

class DungeonRuntime final {
public:
    explicit DungeonRuntime(DungeonRuntimeConfig config);

    [[nodiscard]] bool initialize() noexcept;
    [[nodiscard]] DungeonRuntimeState state() const noexcept;
    [[nodiscard]] dungeon::DungeonSession* session() noexcept;
    [[nodiscard]] const dungeon::DungeonSession* session() const noexcept;
    [[nodiscard]] DungeonRenderStatus render_status() const noexcept;
    void service_pending_transition() noexcept;
    [[nodiscard]] bool recover_with_new_run() noexcept;

private:
    [[nodiscard]] std::optional<std::uint64_t> select_new_run_seed() const noexcept;
    void sync_load_status(const persistence::SaveLoadResult& result) noexcept;
    void sync_commit_status(const persistence::SaveCommitResult& result) noexcept;
    [[nodiscard]] static dungeon::TransitionSaveResult to_session_result(
        const persistence::SaveCommitResult& saved) noexcept;

    DungeonRuntimeConfig config_{};
    persistence::SaveStore store_;
    std::optional<dungeon::DungeonSession> session_{};
    DungeonRuntimeState state_{DungeonRuntimeState::uninitialized};
    DungeonRenderStatus status_{};
};

}  // namespace arpg::platform
