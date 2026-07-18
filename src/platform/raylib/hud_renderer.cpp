#include "combat_renderer.hpp"

#include "combat_view_math.hpp"
#include "control_hints.hpp"
#include "dungeon_runtime.hpp"
#include "dungeon_view_math.hpp"
#include "hud_font.hpp"
#include "hud_palette.hpp"
#include "hud_renderer.hpp"
#include "render_layout.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
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

bool has_requested_glyphs(Font font,
    const DeathOverlayFontPlan& plan) noexcept {
    if (!IsFontValid(font)
            || font.glyphCount < static_cast<int>(plan.codepoint_count)
            || font.glyphs == nullptr) {
        return false;
    }
    for (std::size_t requested = 0U; requested < plan.codepoint_count;
         ++requested) {
        bool found = false;
        for (int glyph = 0; glyph < font.glyphCount; ++glyph) {
            if (font.glyphs[glyph].value == plan.codepoints[requested]) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

[[nodiscard]] float clamped_ratio(float ratio) noexcept {
    if (!std::isfinite(ratio)) return 0.0F;
    return std::clamp(ratio, 0.0F, 1.0F);
}

[[nodiscard]] HudRect player_bar_bounds(const HudLayout& layout,
    float logical_y) noexcept {
    const float scale = layout.scale;
    return {
        layout.player_panel.x + (94.0F * scale),
        layout.player_panel.y + (logical_y * scale),
        std::max(0.0F, layout.player_panel.width - (106.0F * scale)),
        std::max(0.0F, 14.0F * scale),
    };
}

[[nodiscard]] Color color_for_bar(HudBarKind kind) noexcept {
    switch (kind) {
    case HudBarKind::health: return hud_palette_color(HudPaletteId::health);
    case HudBarKind::barrier: return hud_palette_color(HudPaletteId::barrier);
    case HudBarKind::experience: return hud_palette_color(HudPaletteId::experience);
    }
    return hud_palette().text;
}

void draw_player_bar(const HudBarPlan& bar) noexcept {
    const Color fill = color_for_bar(bar.kind);
    const Rectangle bounds{bar.bounds.x, bar.bounds.y, bar.bounds.width,
        bar.bounds.height};
    DrawRectangleRec(bounds, Color{20, 23, 31, 230});
    DrawRectangleRec({bar.bounds.x + 1.0F, bar.bounds.y + 1.0F,
        std::max(0.0F, bar.bounds.width - 2.0F) * clamped_ratio(bar.ratio),
        std::max(0.0F, bar.bounds.height - 2.0F)}, fill);
    DrawRectangleLinesEx(bounds, 1.0F, Color{8, 10, 16, 255});
}

}  // namespace

Color hud_palette_color(HudPaletteId palette_id) noexcept {
    const HudPalette palette = hud_palette();
    switch (palette_id) {
    case HudPaletteId::health: return palette.health;
    case HudPaletteId::barrier: return palette.barrier;
    case HudPaletteId::experience: return palette.experience;
    }
    return palette.text;
}

namespace {

[[nodiscard]] const char* status_tag_label(HudStatusTagKind tag) noexcept {
    switch (tag) {
    case HudStatusTagKind::slow: return u8"减速";
    case HudStatusTagKind::corrosion: return u8"腐蚀";
    case HudStatusTagKind::invulnerable: return u8"无敌";
    }
    return "";
}

}  // namespace

PlayerPanelPlan make_player_panel_plan(const PlayerHudModel& player,
    const HudLayout& layout, float presentation_seconds) noexcept {
    PlayerPanelPlan plan{};
    if (!player.visible || layout.player_panel.width <= 0.0F
            || layout.player_panel.height <= 0.0F || layout.scale <= 0.0F) {
        return plan;
    }

    plan.bars[plan.bar_count++] = {
        player_bar_bounds(layout, 26.0F), clamped_ratio(player.hp_ratio),
        HudBarKind::health};
    if (player.max_barrier > 0) {
        plan.bars[plan.bar_count++] = {
            player_bar_bounds(layout, 52.0F), clamped_ratio(player.barrier_ratio),
            HudBarKind::barrier};
    }
    plan.experience_maxed = player.level >= 100U;
    plan.bars[plan.bar_count++] = {
        player_bar_bounds(layout, 78.0F),
        plan.experience_maxed ? 1.0F : clamped_ratio(player.experience_ratio),
        HudBarKind::experience};

    plan.tag_count = std::min<std::uint8_t>(player.status_tag_count,
        static_cast<std::uint8_t>(plan.tags.size()));
    for (std::size_t index = 0U; index < plan.tag_count; ++index) {
        plan.tags[index] = player.status_tags[index];
    }

    const float seconds = std::max(0.0F, presentation_seconds);
    const float pulse_phase = std::fmod(seconds, 0.5F);
    plan.low_health_emphasis = std::isfinite(player.hp_ratio)
        && clamped_ratio(player.hp_ratio) < 0.25F
        && pulse_phase < 0.25F;
    return plan;
}

HudRenderer::~HudRenderer() noexcept {
    shutdown();
}

bool HudRenderer::initialize() noexcept {
    shutdown();
    const HudFontPlan plan = hud_font_plan();
    if (!plan.covers_required_text) {
        TraceLog(LOG_WARNING, "HUD: Shared font plan misses required Chinese text");
        font_ = GetFontDefault();
        return false;
    }

    for (std::size_t index = 0U; index < plan.shared.candidate_count; ++index) {
        const char* path = plan.shared.candidate_paths[index];
        if (path == nullptr || !FileExists(path)) continue;
        Font candidate = LoadFontEx(path, 32, plan.shared.codepoints.data(),
            static_cast<int>(plan.shared.codepoint_count));
        if (has_requested_glyphs(candidate, plan.shared)) {
            font_ = candidate;
            font_ready_ = true;
            TraceLog(LOG_INFO, "HUD: Loaded Chinese font %s (%i glyphs)",
                path, candidate.glyphCount);
            return true;
        }
        if (IsFontValid(candidate)) UnloadFont(candidate);
    }

    font_ = GetFontDefault();
    TraceLog(LOG_WARNING,
        "HUD: No complete Chinese font found; using default font fallback");
    return false;
}

void HudRenderer::shutdown() noexcept {
    if (font_ready_ && IsWindowReady()) UnloadFont(font_);
    font_ = {};
    font_ready_ = false;
}

bool HudRenderer::font_ready() const noexcept {
    return font_ready_;
}

void HudRenderer::draw(const HudViewModel& view,
    const HudLayout& layout) const noexcept {
    if (!font_ready_ || !IsWindowReady()) return;
    const PlayerPanelPlan plan = make_player_panel_plan(view.player, layout,
        static_cast<float>(GetTime()));
    if (plan.bar_count == 0U) return;

    const HudPalette palette = hud_palette();
    DrawRectangleRounded({layout.player_panel.x, layout.player_panel.y,
        layout.player_panel.width, layout.player_panel.height}, 0.08F, 6,
        Color{7, 10, 17, 220});

    char text[96]{};
    for (std::size_t index = 0U; index < plan.bar_count; ++index) {
        const HudBarPlan& bar = plan.bars[index];
        switch (bar.kind) {
        case HudBarKind::health:
            std::snprintf(text, sizeof(text), u8"生命 HP %d/%d", view.player.hp,
                view.player.max_hp);
            break;
        case HudBarKind::barrier:
            std::snprintf(text, sizeof(text), u8"护盾 %d/%d", view.player.barrier,
                view.player.max_barrier);
            break;
        case HudBarKind::experience:
            if (plan.experience_maxed) {
                std::snprintf(text, sizeof(text), "XP MAX");
            } else {
                std::snprintf(text, sizeof(text), "XP %llu/%llu",
                    static_cast<unsigned long long>(view.player.experience),
                    static_cast<unsigned long long>(view.player.required_experience));
            }
            break;
        }
        DrawTextEx(font_, text, {layout.player_panel.x + (12.0F * layout.scale),
            bar.bounds.y - (2.0F * layout.scale)}, 15.0F * layout.scale,
            1.0F * layout.scale, palette.text);
        draw_player_bar(bar);
    }
    if (plan.low_health_emphasis) {
        DrawRectangleLinesEx({layout.player_panel.x, layout.player_panel.y,
            layout.player_panel.width, layout.player_panel.height},
            2.0F * layout.scale,
            palette.health);
    }
    for (std::size_t index = 0U; index < plan.tag_count; ++index) {
        const float tag_x = layout.player_panel.x
            + ((12.0F + static_cast<float>(index) * 64.0F) * layout.scale);
        const float tag_y = layout.player_panel.y + (110.0F * layout.scale);
        DrawRectangleRounded({tag_x, tag_y, 56.0F * layout.scale,
            20.0F * layout.scale}, 0.18F, 4, Color{30, 39, 55, 235});
        DrawTextEx(font_, status_tag_label(plan.tags[index]),
            {tag_x + (8.0F * layout.scale), tag_y + (2.0F * layout.scale)},
            13.0F * layout.scale, 1.0F * layout.scale, palette.text);
    }
}

void CombatRenderer::draw_abyss_hud(
    const dungeon::DungeonSnapshot& current,
    float x,
    int& y,
    int line_step) const noexcept {
    const AbyssHudValues abyss = abyss_hud_values(current);
    if (!abyss.visible) return;

    constexpr Color kAbyss{255, 126, 206, 255};
    constexpr Color kReward{255, 211, 111, 255};
    DrawText(TextFormat("%s  Rule %s",
        abyss.danger_label, abyss.rule_label),
        static_cast<int>(x), y, 16, kAbyss);
    y += line_step;
    DrawText(abyss.effect_label, static_cast<int>(x), y, 14,
        Color{232, 190, 220, 255});
    y += line_step;
    DrawText(TextFormat("Reward pending %u / unpicked %u",
        static_cast<unsigned>(abyss.pending_rewards),
        static_cast<unsigned>(abyss.unpicked_rewards)),
        static_cast<int>(x), y, 16, kReward);
    y += line_step;

    if (!abyss.confirmation_visible) return;
    const int font_size = 18;
    const bool door_confirmation = abyss.confirmation_transition
        == dungeon::TransitionKind::door;
    const char* direction = door_confirmation
        ? exit_direction_label(abyss.confirmation_direction) : "";
    constexpr const char* kDoorSeparator = " door: ";
    const int direction_width = door_confirmation
        ? MeasureText(direction, font_size) : 0;
    const int separator_width = door_confirmation
        ? MeasureText(kDoorSeparator, font_size) : 0;
    const int prompt_width = direction_width + separator_width
        + MeasureText(abyss.confirmation_label, font_size);
    const float panel_width = static_cast<float>(prompt_width + 36);
    const float panel_x = (static_cast<float>(GetScreenWidth()) - panel_width)
        * 0.5F;
    const float panel_y = static_cast<float>(GetScreenHeight() - 72);
    DrawRectangleRounded({panel_x, panel_y, panel_width, 42.0F},
        0.18F, 6, Color{45, 8, 34, 238});
    DrawRectangleRoundedLines({panel_x, panel_y, panel_width, 42.0F},
        0.18F, 6, Color{255, 98, 190, 255});
    int prompt_x = static_cast<int>(panel_x + 18.0F);
    if (door_confirmation) {
        DrawText(direction, prompt_x, static_cast<int>(panel_y + 11.0F),
            font_size, kAbyss);
        prompt_x += direction_width;
        DrawText(kDoorSeparator, prompt_x,
            static_cast<int>(panel_y + 11.0F), font_size, RAYWHITE);
        prompt_x += separator_width;
    }
    DrawText(abyss.confirmation_label, prompt_x,
        static_cast<int>(panel_y + 11.0F), font_size, RAYWHITE);
}

void CombatRenderer::draw_hud(
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& runtime_status,
    bool draw_debug,
    const ControlHints& control_hints) const noexcept {
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
    DrawText(control_hints.primary.data(),
        static_cast<int>(layout.hud_x), y, 16, accent);
    y = layout.hud_second_instruction_y;
    DrawText(control_hints.secondary.data(),
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
    DrawText(TextFormat("Inventory %u  Ground items %u",
        static_cast<unsigned>(current.inventory_count),
        static_cast<unsigned>(current.ground_item_count)),
        static_cast<int>(layout.hud_x), y, 16, text);
    y += layout.hud_line_step;
    if (current.combat.has_value()) {
        const combat::CombatSnapshot& combat_state = *current.combat;
        DrawText(TextFormat("HP %d/%d", combat_state.player.hp,
            combat_state.player.max_hp), static_cast<int>(layout.hud_x), y, 16, text);
        draw_bar(126.0F, static_cast<float>(y + 5), 150.0F,
            player_hp_ratio(combat_state.player), Color{77, 215, 127, 255});
        y += layout.hud_line_step;
        if (combat_state.player.max_barrier > 0) {
            DrawText(TextFormat("Barrier %d/%d", combat_state.player.barrier,
                combat_state.player.max_barrier), static_cast<int>(layout.hud_x), y, 16,
                Color{119, 191, 255, 255});
            draw_bar(126.0F, static_cast<float>(y + 5), 150.0F,
                combat_state.player.max_barrier <= 0 ? 0.0F
                    : static_cast<float>(combat_state.player.barrier)
                        / static_cast<float>(combat_state.player.max_barrier),
                Color{119, 191, 255, 255});
            y += layout.hud_line_step;
        }
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
    draw_abyss_hud(current, layout.hud_x, y, layout.hud_line_step);
    DrawText(TextFormat("ABYSS %s  Hole %s  Autosave %s",
        current.is_abyss ? "YES" : "NO", current.has_hole
            ? (current.phase == dungeon::RoomPhase::committing ? "SAVING"
                : doors_open ? "READY" : "SEALED") : "NONE",
        save_indicator_label(runtime_status.indicator)),
        static_cast<int>(layout.hud_x), y, 16,
        runtime_status.indicator == SaveIndicator::error ? Color{255, 120, 120, 255} : text);
}

}  // namespace arpg::platform
