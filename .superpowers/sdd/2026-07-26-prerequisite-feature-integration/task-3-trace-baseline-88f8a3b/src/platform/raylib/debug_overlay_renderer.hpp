#pragma once

#include "combat_feedback.hpp"
#include "dungeon_runtime.hpp"
#include "hud_view_model.hpp"

#include <cstdint>
#include <cstddef>

namespace arpg::platform {

struct DebugOverlayDiagnosticsPlan final {
    std::uint8_t total_budget{};
    std::uint8_t current_wave_budget{};
    std::size_t active_monsters{};
    std::size_t active_projectiles{};
    std::size_t active_hazards{};
    std::uint32_t projectile_saturation{};
    std::uint32_t projectile_invalid_owner{};
    std::uint32_t hazard_saturation{};
    std::uint32_t hazard_invalid_owner{};
    std::uint32_t ground_saturation{};
    bool room_index_overflow{};
    HudBuildDiagnostics hud{};
    std::uint32_t notice_drops{};
    std::uint64_t binding_revision{};
    combat::CombatEvent last_event{};
    bool has_last_event{};
    bool cjk_font_ready{};
};

[[nodiscard]] DebugOverlayDiagnosticsPlan make_debug_overlay_diagnostics_plan(
    const dungeon::DungeonSnapshot&, const HudBuildDiagnostics&,
    std::uint32_t notice_drops, std::uint64_t binding_revision,
    const combat::CombatEvent&, bool has_last_event,
    bool cjk_font_ready) noexcept;

class DebugOverlayRenderer final {
public:
    void draw(const dungeon::DungeonSnapshot&, const DungeonRenderStatus&,
        const CombatFeedback&, bool audio_ready,
        const DebugOverlayDiagnosticsPlan&) const noexcept;
};

}  // namespace arpg::platform
