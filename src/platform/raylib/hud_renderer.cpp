#include "combat_renderer.hpp"

#include "combat_view_math.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "render_layout.hpp"

#include <raylib.h>

#include <limits>

namespace arpg::platform {
namespace {

const char* ecology_name(dungeon::DungeonElement element) noexcept {
    switch (element) {
    case dungeon::DungeonElement::fire: return "FIRE";
    case dungeon::DungeonElement::water: return "WATER";
    case dungeon::DungeonElement::lightning: return "LIGHTNING";
    case dungeon::DungeonElement::chaos: return "CHAOS";
    }
    return "UNKNOWN";
}

void draw_bar(float x, float y, float width, float ratio, Color color) noexcept {
    if (ratio < 0.0F) ratio = 0.0F;
    else if (ratio > 1.0F) ratio = 1.0F;
    DrawRectangleRec({x, y, width, 5.0F}, Color{20, 23, 31, 230});
    DrawRectangleRec({x + 1.0F, y + 1.0F, (width - 2.0F) * ratio, 3.0F}, color);
}

}  // namespace

void CombatRenderer::draw_hud(
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    bool draw_debug) const noexcept {
    const RenderLayout layout = render_layout(draw_debug);
    const std::uint64_t room_ordinal = current.room_index
            == (std::numeric_limits<std::uint64_t>::max)()
        ? current.room_index : current.room_index + 1U;
    const bool doors_open = current.phase == dungeon::RoomPhase::cleared
        || current.phase == dungeon::RoomPhase::awaiting_exit;
    DrawRectangleRounded({16.0F, 14.0F, layout.hud_panel_width,
        layout.hud_panel_height}, 0.06F, 6, Color{7, 10, 17, 220});
    const Color text{218, 226, 239, 255};
    const Color accent{110, 207, 255, 255};
    int y = layout.hud_first_line_y;
    DrawText("WASD Move  J Light  K Jump  L Launcher  E Descend",
        static_cast<int>(layout.hud_x), y, 16, accent);
    y = layout.hud_second_instruction_y;
    DrawText("R Reset  F1 Debug  F12 Screenshot  Esc Exit",
        static_cast<int>(layout.hud_x), y, 16, accent);
    y = layout.hud_status_y;
    DrawText(TextFormat("Depth %llu  Floor Room %llu  Global Room %llu",
        static_cast<unsigned long long>(current.depth),
        static_cast<unsigned long long>(current.floor_room_index),
        static_cast<unsigned long long>(room_ordinal)),
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    DrawText(TextFormat("Ecology %s  F/W/L/C bias %u/%u/%u/%u",
        ecology_name(current.ecology), current.biases[0], current.biases[1],
        current.biases[2], current.biases[3]), static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    if (current.combat.has_value()) {
        const combat::CombatSnapshot& combat_state = *current.combat;
        DrawText(TextFormat("HP %d/%d", combat_state.player.hp,
            combat_state.player.max_hp), static_cast<int>(layout.hud_x), y, 16, text);
        draw_bar(126.0F, static_cast<float>(y + 5), 150.0F,
            player_hp_ratio(combat_state.player), Color{77, 215, 127, 255});
        y += layout.hud_line_step;
        static const progression::ProgressionRules kProgressionRules =
            progression::default_progression_rules();
        const ProgressionHudValues progression = progression_hud_values(current,
            kProgressionRules);
        DrawText(TextFormat("Level %u/100  Passive Points %u",
            static_cast<unsigned>(progression.level),
            static_cast<unsigned>(progression.unspent_passive_points)),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        if (progression.maximum_level) {
            DrawText("XP MAX", static_cast<int>(layout.hud_x), y, 16, accent);
        } else {
            DrawText(TextFormat("XP %llu/%llu  Room Pending +%llu",
                static_cast<unsigned long long>(progression.experience),
                static_cast<unsigned long long>(progression.required_experience),
                static_cast<unsigned long long>(progression.pending_room_experience)),
                static_cast<int>(layout.hud_x), y, 16, accent);
        }
        y += layout.hud_line_step;
        const unsigned wave_current = current.wave_count == 0U ? 0U
            : static_cast<unsigned>(current.wave_index) + 1U;
        DrawText(TextFormat("Wave %u/%u  Budget %u/%u  Targets %u", wave_current,
            static_cast<unsigned>(current.wave_count),
            static_cast<unsigned>(current.encounter.current_wave_budget),
            static_cast<unsigned>(current.encounter.total_budget),
            static_cast<unsigned>(current.remaining_targets)),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Active M/P/H %u/%u/%u",
            static_cast<unsigned>(combat_state.monster_count),
            static_cast<unsigned>(combat_state.projectile_count),
            static_cast<unsigned>(combat_state.hazard_count)),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
        DrawText(TextFormat("Saturation P/H %u/%u  Invalid owner P/H %u/%u",
            combat_state.diagnostics.projectile_saturation_count,
            combat_state.diagnostics.hazard_saturation_count,
            combat_state.diagnostics.projectile_invalid_owner_count,
            combat_state.diagnostics.hazard_invalid_owner_count),
            static_cast<int>(layout.hud_x), y, 16, text);
        y += layout.hud_line_step;
    } else {
        DrawText("HP / wave / encounter diagnostics unavailable",
            static_cast<int>(layout.hud_x), y, 16, Color{255, 151, 117, 255});
        y += layout.hud_line_step;
    }
    DrawText(TextFormat("ABYSS %s  Hole %s  Autosave %s",
        current.is_abyss ? "YES" : "NO", current.has_hole
            ? (current.phase == dungeon::RoomPhase::committing ? "SAVING"
                : doors_open ? "READY" : "SEALED") : "NONE",
        save_indicator_label(runtime_status.indicator)),
        static_cast<int>(layout.hud_x), y, 16,
        runtime_status.indicator == SaveIndicator::error ? Color{255, 120, 120, 255} : text);
}

}  // namespace arpg::platform
