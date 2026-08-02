#pragma once

#include "combat/combat_types.hpp"
#include "hud_layout.hpp"
#include "hud_notice_state.hpp"
#include "hud_view_model.hpp"

#include <cstdint>

namespace arpg::dungeon {
struct DungeonSnapshot;
}

namespace arpg::settings {
struct SettingsData;
}

namespace arpg::platform {
struct PhysicalKeySnapshot;
struct RaylibHostConfig;
enum class Stage11CHudValidationScenario : std::uint8_t;

namespace host_validation {

struct Stage11CHudValidationState final {
    std::uint32_t injected_frames{};
    std::uint32_t target_presented_frames{};
    combat::Vec3 previous_player_position{};
    std::uint16_t stalled_movement_frames{};
    std::uint16_t recovery_movement_frames{};
    std::uint8_t recovery_direction{};
    bool previous_player_position_valid{};
    bool movement_was_requested{};
    std::uint64_t production_snapshot_hash{};
    HudViewModel model{};
    HudNoticeView notices{};
    HudLayout layout{};
    bool cjk_font_ready{};
    bool debug_visible{};
    bool captured{};
};

[[nodiscard]] PhysicalKeySnapshot inject_stage11c_physical_edges(
    PhysicalKeySnapshot, const RaylibHostConfig&,
    const settings::SettingsData&, const dungeon::DungeonSnapshot&,
    Stage11CHudValidationState&) noexcept;
[[nodiscard]] std::uint64_t stage11c_production_snapshot_hash(
    const dungeon::DungeonSnapshot&) noexcept;
[[nodiscard]] bool stage11c_hud_validation_reached(
    const dungeon::DungeonSnapshot&, Stage11CHudValidationScenario,
    const Stage11CHudValidationState&, bool draw_debug) noexcept;
void write_stage11c_hud_validation_summary(
    const RaylibHostConfig&, const Stage11CHudValidationState&) noexcept;

}  // namespace host_validation
}  // namespace arpg::platform
