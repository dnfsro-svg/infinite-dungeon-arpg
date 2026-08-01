#pragma once

#include "platform/settings/settings_store.hpp"

#include <cstdint>

namespace arpg::dungeon {
struct DungeonSnapshot;
}

namespace arpg::platform {
struct PhysicalKeySnapshot;
struct PauseMenuState;
struct RaylibHostConfig;

namespace host_validation {

struct Stage11BValidationState final {
    std::uint32_t injected_frame{};
    std::uint32_t paused_presented{};
    std::uint64_t fixed_ticks{};
    std::uint64_t paused_ticks_before{};
    std::uint64_t paused_ticks_after{};
    std::uint64_t resume_ticks_before{};
    std::uint64_t resume_ticks_after{};
    std::uint64_t player_monster_hash_before{};
    std::uint64_t player_monster_hash_after{};
    std::uint32_t old_attack_count{};
    std::uint32_t new_attack_count{};
    bool old_attack_checked{};
    bool pause_capture_while_paused{};
    bool resume_input_injected{};
    bool resume_observed{};
    bool recovery_notice_visible{};
    settings::SettingsLoadStatus load_status{
        settings::SettingsLoadStatus::defaults_missing};
};

[[nodiscard]] PhysicalKeySnapshot inject_stage11b_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    Stage11BValidationState&) noexcept;
[[nodiscard]] bool stage11b_validation_complete(
    const RaylibHostConfig&, const Stage11BValidationState&,
    const PauseMenuState&) noexcept;
[[nodiscard]] std::uint64_t stage11b_snapshot_hash(
    const dungeon::DungeonSnapshot&) noexcept;
void write_stage11b_validation_summary(
    const RaylibHostConfig&, const Stage11BValidationState&,
    const PauseMenuState&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
